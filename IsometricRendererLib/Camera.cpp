#include "pch.h"
#include "Camera.h"
#include "IsometricRenderer.h"

#include <algorithm>

using namespace tw;

const float Camera::MIN_ZOOM = 0.5f;
const float Camera::MAX_ZOOM = 2.0f;

Camera::Camera()
	: mapWidth(1), mapHeight(1), zoom(1.f), following(false), dragging(false)
{
}

sf::Vector2f Camera::cellToWorld(float cellX, float cellY)
{
	return sf::Vector2f((cellX - cellY) * 60.f + 60.f, (cellX + cellY) * 30.f + 30.f);
}

void Camera::reset(int mapWidth, int mapHeight)
{
	this->mapWidth = std::max(1, mapWidth);
	this->mapHeight = std::max(1, mapHeight);
	center = cellToWorld((this->mapWidth - 1) / 2.f, (this->mapHeight - 1) / 2.f);
	target = center;
	zoom = 1.f;
	dragging = false;
}

void Camera::fit(int mapWidth, int mapHeight, const sf::Vector2u & viewSize, float margin)
{
	reset(mapWidth, mapHeight);
	if (viewSize.x == 0 || viewSize.y == 0)
		return;

	// Emprise de la carte : losange de (largeur + hauteur) x 60 par (largeur + hauteur) x 30 pixels,
	// plus la hauteur des blocs et des décors.
	float pixelWidth = (this->mapWidth + this->mapHeight) * 60.f;
	float pixelHeight = (this->mapWidth + this->mapHeight) * 30.f + 80.f;
	float needed = std::max(pixelWidth / viewSize.x, pixelHeight / viewSize.y) * margin;
	zoom = std::min(MAX_ZOOM, std::max(MIN_ZOOM, needed));
}

bool Camera::handleEvent(const sf::Event & event, const sf::RenderWindow & window)
{
	sf::Vector2f half(window.getSize().x / 2.f, window.getSize().y / 2.f);

	if (event.type == sf::Event::MouseWheelScrolled)
	{
		// Zoom centré sur la souris : le point du monde sous le curseur ne bouge pas.
		sf::Vector2f mouse((float)event.mouseWheelScroll.x, (float)event.mouseWheelScroll.y);
		sf::Vector2f world = center + (mouse - half) * zoom;

		float factor = event.mouseWheelScroll.delta > 0 ? 1.f / 1.12f : 1.12f;
		zoom = std::min(MAX_ZOOM, std::max(MIN_ZOOM, zoom * factor));

		center = world - (mouse - half) * zoom;
		clampCenter();
		target = center;
		return true;
	}

	if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Right)
	{
		dragging = true;
		lastMouse = sf::Vector2i(event.mouseButton.x, event.mouseButton.y);
		return true;
	}

	if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Right)
	{
		dragging = false;
		return true;
	}

	if (event.type == sf::Event::MouseMoved && dragging)
	{
		sf::Vector2i mouse(event.mouseMove.x, event.mouseMove.y);
		sf::Vector2f delta((float)(mouse.x - lastMouse.x), (float)(mouse.y - lastMouse.y));
		lastMouse = mouse;

		// Déplacer la carte à la main interrompt le suivi.
		following = false;
		center -= delta * zoom;
		clampCenter();
		target = center;
		return true;
	}

	if (event.type == sf::Event::LostFocus)
		dragging = false;

	return false;
}

void Camera::followCell(float cellX, float cellY)
{
	if (following && !dragging)
		target = cellToWorld(cellX, cellY);
}

void Camera::centerOn(float cellX, float cellY)
{
	center = cellToWorld(cellX, cellY);
	clampCenter();
	target = center;
}

void Camera::setZoom(float value)
{
	zoom = std::min(MAX_ZOOM, std::max(MIN_ZOOM, value));
}

void Camera::update(float deltatime)
{
	if (!following)
		return;

	float t = std::min(1.f, deltatime * 4.f);
	center += (target - center) * t;
	clampCenter();
}

void Camera::clampCenter()
{
	// La caméra ne peut pas quitter l'emprise de la carte.
	float left = cellToWorld(0.f, (float)(mapHeight - 1)).x - 60.f;
	float right = cellToWorld((float)(mapWidth - 1), 0.f).x + 60.f;
	float top = cellToWorld(0.f, 0.f).y - 30.f;
	float bottom = cellToWorld((float)(mapWidth - 1), (float)(mapHeight - 1)).y + 30.f;

	center.x = std::min(right, std::max(left, center.x));
	center.y = std::min(bottom, std::max(top, center.y));
}

void Camera::apply(IsometricRenderer & renderer) const
{
	renderer.setCamera(center, zoom);
}
