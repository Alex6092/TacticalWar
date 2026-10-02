#include "pch.h"
#include "SpellView.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

std::map<std::string, std::unique_ptr<SpellView::Sheet>> SpellView::sheets;

namespace
{
	std::string trim(const std::string & text)
	{
		std::size_t start = text.find_first_not_of(" \t\r");
		std::size_t end = text.find_last_not_of(" \t\r");
		return start == std::string::npos ? std::string() : text.substr(start, end - start + 1);
	}

	bool readPair(const std::string & value, int & a, int & b)
	{
		char comma = 0;
		std::istringstream stream(value);
		return (bool)(stream >> a >> comma >> b) && comma == ',';
	}
}

SpellView::SpellView(int x, int y)
	: AbstractSpellView<sf::Sprite*>(x, y), sheet(NULL), elapsed(0), fps(24.f), loop(false),
	posX((float)x), posY((float)y), height(0), scale(1.f), rotation(0), anchorX(0.5f), anchorY(0.5f),
	color(sf::Color::White), layer(Layer::TOP), additive(false)
{
}

SpellView::~SpellView()
{
}

const SpellView::Sheet * SpellView::loadSheet(const std::string & filename)
{
	auto cached = sheets.find(filename);
	if (cached != sheets.end())
		return cached->second.get();

	// Atlas libGDX : en-tête (nom de l'image, size, format…), puis une région par image :
	// son nom, puis des lignes indentées "clé: valeur" (rotate, xy, size, orig, offset, index).
	struct Region
	{
		Frame frame;
		int index = 0;
		bool hasRect = false;
		bool rotated = false;
		int origW = 0, origH = 0, offX = 0, offY = 0;
	};

	std::vector<Region> regions;
	std::ifstream file(filename + ".txt");
	std::string line;
	bool inRegion = false;
	while (std::getline(file, line))
	{
		std::string content = trim(line);
		if (content.empty())
		{
			inRegion = false;
			continue;
		}

		bool indented = line[0] == ' ' || line[0] == '\t';
		std::size_t colon = content.find(':');
		if (!indented && colon == std::string::npos)
		{
			// Nom d'image de la planche ou nom d'une région.
			if (content.find(".png") == std::string::npos)
			{
				regions.push_back(Region());
				inRegion = true;
			}
			continue;
		}
		if (!inRegion || !indented || colon == std::string::npos)
			continue;

		std::string key = trim(content.substr(0, colon));
		std::string value = trim(content.substr(colon + 1));
		Region & region = regions.back();
		int a = 0, b = 0;
		if (key == "rotate")
			region.rotated = value == "true";
		else if (key == "index")
			region.index = std::atoi(value.c_str());
		else if (readPair(value, a, b))
		{
			if (key == "xy")
			{
				region.frame.rect.left = a;
				region.frame.rect.top = b;
				region.hasRect = true;
			}
			else if (key == "size")
			{
				region.frame.rect.width = a;
				region.frame.rect.height = b;
			}
			else if (key == "orig")
			{
				region.origW = a;
				region.origH = b;
			}
			else if (key == "offset")
			{
				region.offX = a;
				region.offY = b;
			}
		}
	}

	std::unique_ptr<Sheet> loaded(new Sheet());
	std::stable_sort(regions.begin(), regions.end(), [](const Region & a, const Region & b) { return a.index < b.index; });
	for (Region & region : regions)
	{
		if (!region.hasRect || region.frame.rect.width <= 0 || region.frame.rect.height <= 0)
			continue;
		if (region.rotated)
		{
			std::cout << "Animation " << filename << " : image pivotée ignorée" << std::endl;
			continue;
		}

		// L'image d'origine fait orig ; le rectangle (rogné) y est décalé de offset, compté
		// depuis le coin inférieur gauche.
		Frame & frame = region.frame;
		frame.width = (float)(region.origW > 0 ? region.origW : frame.rect.width);
		frame.height = (float)(region.origH > 0 ? region.origH : frame.rect.height);
		frame.offsetX = (float)region.offX;
		frame.offsetY = frame.height - region.offY - frame.rect.height;
		loaded->frames.push_back(frame);
	}

	if (loaded->frames.empty() || !loaded->texture.loadFromFile(filename + ".png"))
	{
		std::cout << "Animation introuvable ou vide : " << filename << std::endl;
		loaded->frames.clear();
	}
	loaded->texture.setSmooth(true);

	const Sheet * result = loaded.get();
	sheets[filename] = std::move(loaded);
	return result;
}

void SpellView::clearCache()
{
	sheets.clear();
}

bool SpellView::preload(const std::string & filename)
{
	return !loadSheet(filename)->frames.empty();
}

bool SpellView::loadAnimation(const std::string & filename)
{
	sheet = loadSheet(filename);
	elapsed = 0;
	if (sheet->frames.empty())
		return false;
	sprite.setTexture(sheet->texture);
	return true;
}

float SpellView::getAnimationDuration() const
{
	if (sheet == NULL || sheet->frames.empty())
		return 0;
	return sheet->frames.size() / fps;
}

bool SpellView::isFinished() const
{
	if (sheet == NULL || sheet->frames.empty())
		return true;
	return !loop && elapsed >= getAnimationDuration();
}

sf::Sprite* SpellView::getImageToDraw()
{
	if (sheet == NULL || sheet->frames.empty())
		return NULL;

	int count = (int)sheet->frames.size();
	int index = (int)std::floor(elapsed * fps);
	index = loop ? index % count : std::min(index, count - 1);
	const Frame & frame = sheet->frames[std::max(0, index)];

	sprite.setTextureRect(frame.rect);
	sprite.setOrigin(anchorX * frame.width - frame.offsetX, anchorY * frame.height - frame.offsetY);
	sprite.setScale(scale, scale);
	sprite.setRotation(rotation);
	sprite.setColor(color);
	return &sprite;
}

void SpellView::update(float deltatime)
{
	elapsed += deltatime;
}
