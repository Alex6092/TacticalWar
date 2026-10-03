#pragma once

#include <SFML/Graphics.hpp>
#include <CellData.h>

namespace tw
{
	class CellColorator
	{
	public:
		// Teinte de la tuile (multipliée par sa texture : elle ne peut que l'assombrir).
		virtual sf::Color getColorForCell(CellData * cell) = 0;
		// Surbrillance dessinée par-dessus la case : losange semi-transparent, lisible sur toutes les
		// tuiles (visée d'un sort…). Transparente par défaut.
		// (sf::Color::Transparent n'est pas résolu par l'éditeur en C++/CLI : couleur construite ici.)
		virtual sf::Color getOverlayForCell(CellData * cell) { return sf::Color(0, 0, 0, 0); }
	};
}