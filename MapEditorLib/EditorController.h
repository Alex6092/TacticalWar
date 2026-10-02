#pragma once

// Logique de l'éditeur de cartes, sans interface (testée dans TacticalWarTests).
// Ce fichier est inclus par l'éditeur C++/CLI : pas de nlohmann, <mutex> ni <thread> ici.

#include <memory>
#include <string>
#include <vector>

namespace tw
{
	class Environment;

	namespace editor
	{
		enum class Tool { PAINT, FILL, RECTANGLE, START_TEAM1, START_TEAM2, ERASE_START };

		struct CellState
		{
			std::string tile;
			int start = 0;

			bool operator==(const CellState & other) const { return tile == other.tile && start == other.start; }
			bool operator!=(const CellState & other) const { return !(*this == other); }
		};

		struct CellChange
		{
			int x = 0;
			int y = 0;
			CellState before;
			CellState after;
		};

		// Action annulable : un trait, un remplissage, un rectangle, ou un redimensionnement
		// (carte complète avant/après au format v2).
		struct EditStep
		{
			int revision = 0;
			std::vector<CellChange> changes;
			std::string mapBefore;
			std::string mapAfter;
		};

		struct ValidationMessage
		{
			bool blocking = false;		// La carte n'est pas jouable
			std::string text;			// UTF-8
			int x = -1;					// Case concernée, -1 si aucune
			int y = -1;
		};

		class EditorController
		{
		public:
			static const int MIN_SIZE = 5;
			static const int MAX_SIZE = 60;

			EditorController();
			~EditorController();

			// Nouvelle carte remplie avec la tuile donnée ; l'historique est vidé.
			void newMap(int width, int height, int id, const std::string & fillTile);
			// Carte chargée (prise en charge par le contrôleur) ; l'historique est vidé.
			void setEnvironment(Environment * environment);
			Environment * getEnvironment() const { return environment.get(); }

			void setTool(Tool tool) { this->tool = tool; }
			Tool getTool() const { return tool; }
			void setTile(const std::string & tile) { this->tile = tile; }
			const std::string & getTile() const { return tile; }

			// Souris : bouton enfoncé sur une case, déplacement bouton enfoncé, bouton relâché.
			// Un trait complet (ou un rectangle) forme une seule action annulable.
			void pointerDown(int x, int y);
			void pointerMove(int x, int y);
			void pointerUp();
			// Rectangle en cours de tracé (aperçu).
			bool getRectangle(int & x0, int & y0, int & x1, int & y1) const;

			bool undo();
			bool redo();
			bool canUndo() const { return !undoStack.empty(); }
			bool canRedo() const { return !redoStack.empty(); }

			// Redimensionne la carte (les cases conservées gardent leur contenu).
			void resize(int width, int height, const std::string & fillTile);

			bool isModified() const;
			void markSaved();

			std::vector<ValidationMessage> validate() const;

			// Dossiers où enregistrer les cartes : celui du jeu (./assets/map/) et, si l'éditeur est
			// lancé depuis le dépôt, le dossier versionné <dépôt>/assets/map/.
			static std::vector<std::string> mapDirectories();

		private:
			CellState stateOf(int x, int y) const;
			void applyState(int x, int y, const CellState & state);
			void change(int x, int y, const CellState & after);
			void commitStep();
			void applyTool(int x, int y);
			void fill(int x, int y);
			void restoreMap(const std::string & json);
			int currentRevision() const;

			std::unique_ptr<Environment> environment;
			Tool tool;
			std::string tile;

			bool active;
			EditStep current;
			int rectX0, rectY0, rectX1, rectY1;

			std::vector<EditStep> undoStack;
			std::vector<EditStep> redoStack;
			int nextRevision;
			int savedRevision;
		};
	}
}
