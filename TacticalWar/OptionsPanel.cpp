#include "OptionsPanel.h"

#include <Palette.h>

#include "ClientConfig.h"
#include "MusicManager.h"
#include "UiScale.h"

using namespace tw;

namespace
{
	const float WIDTH = 620;
	const float HEIGHT = 560;
	const int SCALES[3] = { 100, 115, 130 };

	tgui::Label::Ptr note(const sf::Font & font, const sf::String & value)
	{
		tgui::Label::Ptr label = tgui::Label::create(value);
		label->setInheritedFont(font);
		label->setTextSize(14);
		label->setMaximumTextWidth(WIDTH - 90);
		label->getRenderer()->setTextColor(sf::Color(190, 190, 200));
		return label;
	}

	tgui::CheckBox::Ptr option(const sf::Font & font, const sf::String & value)
	{
		tgui::CheckBox::Ptr box = tgui::CheckBox::create(value);
		box->setInheritedFont(font);
		box->setTextSize(18);
		box->setSize(22, 22);
		box->getRenderer()->setTextColor(sf::Color(240, 240, 240));
		box->getRenderer()->setTextColorHover(sf::Color(255, 215, 0));
		return box;
	}
}

OptionsPanel::OptionsPanel(tgui::Gui * gui, const sf::Font & titleFont)
{
	textFont.loadFromFile("./assets/font/OpenSans-Regular.ttf");

	panel = tgui::Panel::create();
	panel->setSize(WIDTH, HEIGHT);
	panel->getRenderer()->setBackgroundColor(sf::Color(15, 15, 25, 245));
	panel->getRenderer()->setBorders(2);
	panel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	panel->setVisible(false);

	tgui::Label::Ptr title = tgui::Label::create(L"Options");
	title->setInheritedFont(titleFont);
	title->setTextSize(24);
	title->getRenderer()->setTextColor(sf::Color(255, 215, 0));
	title->setPosition(30, 16);
	panel->add(title);

	float y = 66;
	sound = option(textFont, L"Sons et musique");
	sound->setPosition(30, y);
	panel->add(sound);

	y += 46;
	colorblind = option(textFont, L"Mode daltonien");
	colorblind->setPosition(30, y);
	panel->add(colorblind);
	tgui::Label::Ptr colorblindNote = note(textFont, L"Couleurs distinctes pour toutes les formes de daltonisme : équipes bleue et orange, "
		L"cases d'impact hachurées, symbole d'équipe (rond ou triangle) sur les personnages et dans l'ordre du tour.");
	colorblindNote->setPosition(62, y + 28);
	panel->add(colorblindNote);

	y += 108;
	tgui::Label::Ptr scaleTitle = tgui::Label::create(L"Taille du texte");
	scaleTitle->setInheritedFont(textFont);
	scaleTitle->setTextSize(18);
	scaleTitle->getRenderer()->setTextColor(sf::Color(240, 240, 240));
	scaleTitle->setPosition(30, y);
	panel->add(scaleTitle);
	for (int i = 0; i < 3; i++)
	{
		tgui::Button::Ptr button = tgui::Button::create(std::to_string(SCALES[i]) + " %");
		button->setInheritedFont(textFont);
		button->setTextSize(16);
		button->setSize(90, 34);
		button->setPosition(200 + i * 100, y - 4);
		int scale = SCALES[i];
		button->connect("pressed", [this, scale]() {
			ClientConfig::get().textScale = scale;
			changed();
		});
		panel->add(button);
		scaleButtons.push_back(button);
	}
	preview = tgui::Label::create(L"Exemple : Léa lance Boule de feu sur Tom (-18 PV).");
	preview->setInheritedFont(textFont);
	preview->getRenderer()->setTextColor(sf::Color(190, 190, 200));
	preview->setPosition(62, y + 40);
	panel->add(preview);

	y += 96;
	turnAlert = option(textFont, L"Alerte de fin de tour");
	turnAlert->setPosition(30, y);
	panel->add(turnAlert);
	tgui::Label::Ptr alertNote = note(textFont, L"Pendant votre tour, le minuteur clignote et un tic sonne chaque seconde "
		L"pendant les 5 dernières secondes (du temps normal, puis de la réserve).");
	alertNote->setPosition(62, y + 28);
	panel->add(alertNote);

	y += 90;
	seeThrough = option(textFont, L"Voir à travers le décor");
	seeThrough->setPosition(30, y);
	panel->add(seeThrough);
	tgui::Label::Ptr seeThroughNote = note(textFont, L"Un personnage caché derrière un arbre, un rocher ou un mur reste visible : "
		L"l'élément devient transparent autour de lui.");
	seeThroughNote->setPosition(62, y + 28);
	panel->add(seeThroughNote);

	tgui::Button::Ptr close = tgui::Button::create(L"Fermer");
	close->setInheritedFont(titleFont);
	close->setTextSize(16);
	close->setSize(160, 40);
	close->setPosition((WIDTH - 160) / 2, HEIGHT - 56);
	close->connect("pressed", [this]() { hide(); });
	panel->add(close);

	sound->connect("Checked", [this]() { changed(); });
	sound->connect("Unchecked", [this]() { changed(); });
	colorblind->connect("Checked", [this]() { changed(); });
	colorblind->connect("Unchecked", [this]() { changed(); });
	turnAlert->connect("Checked", [this]() { changed(); });
	turnAlert->connect("Unchecked", [this]() { changed(); });
	seeThrough->connect("Checked", [this]() { changed(); });
	seeThrough->connect("Unchecked", [this]() { changed(); });

	gui->add(panel);
	refresh();
}

void OptionsPanel::refresh()
{
	refreshing = true;
	const ClientConfig & config = ClientConfig::get();
	sound->setChecked(config.soundSaved());
	colorblind->setChecked(config.colorblind);
	turnAlert->setChecked(config.turnAlert);
	seeThrough->setChecked(config.seeThrough);
	for (int i = 0; i < 3; i++)
	{
		bool selected = config.textScale == SCALES[i];
		scaleButtons[i]->getRenderer()->setBackgroundColor(selected ? sf::Color(255, 215, 0) : sf::Color(235, 235, 235));
		scaleButtons[i]->getRenderer()->setBorderColor(selected ? sf::Color(255, 255, 255) : sf::Color(60, 60, 60));
	}
	preview->setTextSize(ui::text(15));
	refreshing = false;
}

void OptionsPanel::changed()
{
	if (refreshing)
		return;
	ClientConfig & config = ClientConfig::get();
	bool soundWanted = sound->isChecked();
	if (soundWanted != config.soundSaved())
	{
		config.setSound(soundWanted);
		MusicManager::getInstance()->setEnabled(soundWanted);
	}
	config.colorblind = colorblind->isChecked();
	config.turnAlert = turnAlert->isChecked();
	config.seeThrough = seeThrough->isChecked();
	palette::setColorblind(config.colorblind);
	config.save();
	refresh();
	if (onChange)
		onChange();
}

void OptionsPanel::show()
{
	refresh();
	panel->setVisible(true);
	panel->moveToFront();
}

void OptionsPanel::hide()
{
	panel->setVisible(false);
}

bool OptionsPanel::isVisible() const
{
	return panel->isVisible();
}

void OptionsPanel::layout(const sf::Vector2u & windowSize)
{
	panel->setPosition(std::max(0.f, (windowSize.x - WIDTH) / 2), std::max(0.f, (windowSize.y - HEIGHT) / 2 - 20));
}
