#include <httplib.h>

#include "HttpFrontend.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

namespace
{
	const int MAX_EVENT_STREAMS = 8;
}

struct HttpFrontend::Impl
{
	int port;
	std::string webRoot;
	httplib::Server server;
	std::thread thread;

	// Instantané partagé : remplacé en bloc par le thread de jeu, lu par les threads HTTP.
	std::mutex mutex;
	std::condition_variable changed;
	std::shared_ptr<const std::string> state = std::make_shared<const std::string>("{}");
	std::uint64_t version = 0;
	bool stopping = false;
	std::atomic<int> eventStreams{ 0 };

	std::shared_ptr<const std::string> snapshot(std::uint64_t * versionOut)
	{
		std::lock_guard<std::mutex> lock(mutex);
		if (versionOut != nullptr)
			*versionOut = version;
		return state;
	}

	// Attend une version plus récente que "since" (ou l'expiration du délai).
	std::uint64_t waitForChange(std::uint64_t since, std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> lock(mutex);
		changed.wait_for(lock, timeout, [&] { return stopping || version != since; });
		return version;
	}
};

HttpFrontend::HttpFrontend(int port, const std::string & webRoot)
	: impl(new Impl())
{
	impl->port = port;
	impl->webRoot = webRoot;
}

HttpFrontend::~HttpFrontend()
{
	stop();
}

bool HttpFrontend::start(std::string & error)
{
	Impl * d = impl.get();

	d->server.new_task_queue = [] { return new httplib::ThreadPool(16); };

	if (!d->server.set_mount_point("/", d->webRoot))
	{
		error = "Dossier de la page web introuvable : " + d->webRoot;
		return false;
	}

	d->server.Get("/api/health", [](const httplib::Request &, httplib::Response & res) {
		res.set_content("{\"ok\":true}", "application/json");
	});

	// État complet. Avec ?since=<version>, attend un changement (long-poll, 20 s maximum).
	d->server.Get("/api/state", [d](const httplib::Request & req, httplib::Response & res) {
		if (req.has_param("since"))
		{
			std::uint64_t since = std::strtoull(req.get_param_value("since").c_str(), nullptr, 10);
			d->waitForChange(since, std::chrono::seconds(20));
		}

		std::uint64_t version = 0;
		std::shared_ptr<const std::string> state = d->snapshot(&version);
		res.set_header("Cache-Control", "no-store");
		res.set_header("X-State-Version", std::to_string(version));
		res.set_content(*state, "application/json; charset=utf-8");
	});

	// Server-Sent Events : uniquement le numéro de version ; la page relit /api/state.
	d->server.Get("/api/events", [d](const httplib::Request &, httplib::Response & res) {
		if (d->eventStreams >= MAX_EVENT_STREAMS)
		{
			res.status = 503;
			res.set_content("trop de connexions : utilisez /api/state?since=", "text/plain; charset=utf-8");
			return;
		}

		d->eventStreams++;
		std::shared_ptr<std::uint64_t> lastSent = std::make_shared<std::uint64_t>((std::uint64_t)-1);
		res.set_header("Cache-Control", "no-store");
		res.set_chunked_content_provider("text/event-stream",
			[d, lastSent](std::size_t, httplib::DataSink & sink) {
				std::uint64_t version = d->waitForChange(*lastSent, std::chrono::seconds(15));
				if (d->stopping)
					return false;

				std::string message = version != *lastSent
					? "data: " + std::to_string(version) + "\n\n"
					: ": ping\n\n";		// Commentaire SSE : maintient la connexion ouverte
				*lastSent = version;
				return sink.write(message.data(), message.size());
			},
			[d](bool) { d->eventStreams--; });
	});

	if (!d->server.bind_to_port("0.0.0.0", d->port))
	{
		error = "Le port HTTP " + std::to_string(d->port) + " est déjà utilisé.";
		return false;
	}

	d->thread = std::thread([d] { d->server.listen_after_bind(); });
	return true;
}

void HttpFrontend::stop()
{
	if (!impl)
		return;

	{
		std::lock_guard<std::mutex> lock(impl->mutex);
		impl->stopping = true;
	}
	impl->changed.notify_all();
	impl->server.stop();
	if (impl->thread.joinable())
		impl->thread.join();
}

void HttpFrontend::publish(const std::string & stateJson)
{
	{
		std::lock_guard<std::mutex> lock(impl->mutex);
		impl->state = std::make_shared<const std::string>(stateJson);
		impl->version++;
	}
	impl->changed.notify_all();
}

std::uint64_t HttpFrontend::getVersion() const
{
	std::lock_guard<std::mutex> lock(impl->mutex);
	return impl->version;
}
