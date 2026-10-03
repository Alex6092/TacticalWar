#pragma once

#include <SFML/Graphics.hpp>
#include <SpellView.h>
#include <IsometricRenderer.h>
#include <Camera.h>
#include "EditorEventListener.h"
#include <BaseCharacterModel.h>
#include <Environment.h>
#include <vector>
#include <EnvironmentManager.h>
#include <EditorController.h>
#include <TileRegistry.h>
#include <Windows.h>
#include <AbstractSpellView.h>
#include "SelectablePanel.h"
#include "EnvironmentEditorColorator.h"
#include <algorithm>

// Windows.h définit MessageBox (MessageBoxW) : on veut System::Windows::Forms::MessageBox.
#undef MessageBox

namespace EnvironmentEditor {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	// Conversions entre les chaînes .NET et les chaînes UTF-8 du jeu.
	inline String ^ fromUtf8(const std::string & text)
	{
		array<Byte> ^ bytes = gcnew array<Byte>((int)text.size());
		if (!text.empty())
			System::Runtime::InteropServices::Marshal::Copy(IntPtr((void*)text.data()), bytes, 0, (int)text.size());
		return System::Text::Encoding::UTF8->GetString(bytes);
	}

	inline std::string toUtf8(String ^ text)
	{
		array<Byte> ^ bytes = System::Text::Encoding::UTF8->GetBytes(text);
		std::string result(bytes->Length, '\0');
		if (bytes->Length > 0)
			System::Runtime::InteropServices::Marshal::Copy(bytes, 0, IntPtr(&result[0]), bytes->Length);
		return result;
	}

	/// <summary>
	/// Éditeur de cartes : palette de tuiles, outils (pinceau, remplissage, rectangle, départs),
	/// annuler / rétablir, taille de la carte, validation et enregistrement au format v2.
	/// </summary>
	public ref class EditorUI : public System::Windows::Forms::Form
	{
	public:
		// Options de développement (ligne de commande) : --open <id>, --validate,
		// --screenshot <fichier.png> (capture de la fenêtre puis fermeture).
		static int openMapId = 0;
		static bool validateOnStart = false;
		static String ^ screenshotPath = nullptr;

	private:
		int ticks;
		sf::RenderWindow * window;
		tw::IsometricRenderer * renderer;
		EditorEventListener * eventListener;
		EnvironmentEditorColorator * colorator;
		tw::editor::EditorController * controller;
		tw::Camera * camera;
		std::vector<tw::BaseCharacterModel*> * characters;

		bool metadataModified;
		bool loadingMetadata;
		String ^ lastMessage;
		int hoverX;
		int hoverY;

		System::ComponentModel::IContainer ^ components;
		System::Windows::Forms::Panel ^ sfmlRenderingSurface;
		System::Windows::Forms::Timer ^ renderTimer;
		System::Windows::Forms::Panel ^ sidePanel;

		ComboBox ^ mapList;
		Button ^ openButton;
		Button ^ newButton;
		Button ^ saveButton;
		Button ^ saveAsButton;
		TextBox ^ nameBox;
		NumericUpDown ^ widthBox;
		NumericUpDown ^ heightBox;
		Button ^ resizeButton;
		CheckBox ^ poolBox;

		RadioButton ^ paintTool;
		RadioButton ^ fillTool;
		RadioButton ^ rectangleTool;
		RadioButton ^ team1Tool;
		RadioButton ^ team2Tool;
		RadioButton ^ eraseStartTool;
		RadioButton ^ zoneTool;
		RadioButton ^ eraseZoneTool;
		Button ^ undoButton;
		Button ^ redoButton;

		ListView ^ palette;
		ImageList ^ thumbnails;
		Button ^ reloadTilesButton;

		Button ^ validateButton;
		ListBox ^ validationList;
		Label ^ statusLabel;

	public:
		EditorUI(void)
		{
			metadataModified = false;
			loadingMetadata = false;
			lastMessage = L"Clic gauche : outil   Molette : zoom   Clic droit : déplacer la vue";
			hoverX = -1;
			hoverY = -1;

			controller = new tw::editor::EditorController();
			camera = new tw::Camera();
			characters = new std::vector<tw::BaseCharacterModel*>();

			InitializeComponent();

			window = new sf::RenderWindow((sf::WindowHandle)sfmlRenderingSurface->Handle.ToPointer());
			renderer = new tw::IsometricRenderer(window);
			eventListener = new EditorEventListener(this);
			renderer->addEventListener(eventListener);
			colorator = new EnvironmentEditorColorator(controller);
			renderer->setColorator(colorator);

			fillPalette();
			refreshMapList();
			newMap();
			ticks = 0;

			if (openMapId > 0)
			{
				tw::Environment * environment = loadMapFile(openMapId);
				if (environment != NULL)
				{
					controller->setEnvironment(environment);
					showMetadata();
					setStatus(String::Format(L"Carte {0} ouverte.", openMapId));
				}
			}
			if (validateOnStart)
				showValidation(controller->validate());
		}

		//----------------------------------------------------------
		// Événements du rendu (voir EditorEventListener)
		//----------------------------------------------------------

		void onCellPressed(int x, int y)
		{
			controller->pointerDown(x, y);
		}

		void onCellDragged(int x, int y)
		{
			controller->pointerMove(x, y);
		}

		void onCellHover(int x, int y)
		{
			hoverX = x;
			hoverY = y;
			colorator->setHover(x, y);
		}

		void onSfmlEvent(const sf::Event & event)
		{
			if (event.type == sf::Event::MouseButtonReleased && event.mouseButton.button == sf::Mouse::Left)
				controller->pointerUp();
			else if (event.type == sf::Event::MouseLeft)
				colorator->setHover(-1, -1);
			camera->handleEvent(event, *window);
		}

	protected:
		~EditorUI()
		{
			if (components)
				delete components;
		}

		// Raccourcis : Ctrl+Z, Ctrl+Y, Ctrl+S.
		virtual bool ProcessCmdKey(System::Windows::Forms::Message % msg, Keys keyData) override
		{
			if (keyData == (Keys::Control | Keys::Z))
			{
				undo();
				return true;
			}
			if (keyData == (Keys::Control | Keys::Y))
			{
				redo();
				return true;
			}
			if (keyData == (Keys::Control | Keys::S))
			{
				saveMap(false);
				return true;
			}
			return Form::ProcessCmdKey(msg, keyData);
		}

	private:
		//----------------------------------------------------------
		// Construction de l'interface
		//----------------------------------------------------------

		Button ^ createButton(String ^ text, int x, int y, int width, EventHandler ^ handler)
		{
			Button ^ button = gcnew Button();
			button->Text = text;
			button->Location = Point(x, y);
			button->Size = System::Drawing::Size(width, 28);
			button->Click += handler;
			return button;
		}

		Label ^ createLabel(String ^ text, int x, int y)
		{
			Label ^ label = gcnew Label();
			label->Text = text;
			label->Location = Point(x, y + 4);
			label->AutoSize = true;
			return label;
		}

		RadioButton ^ createTool(String ^ text, int x, int y)
		{
			RadioButton ^ radio = gcnew RadioButton();
			radio->Text = text;
			radio->Location = Point(x, y);
			radio->AutoSize = true;
			radio->CheckedChanged += gcnew EventHandler(this, &EditorUI::onToolChanged);
			return radio;
		}

		void InitializeComponent(void)
		{
			this->components = gcnew System::ComponentModel::Container();
			this->SuspendLayout();

			// Panneau latéral.
			sidePanel = gcnew Panel();
			sidePanel->Dock = DockStyle::Left;
			sidePanel->Width = 330;
			sidePanel->AutoScroll = true;
			sidePanel->Padding = System::Windows::Forms::Padding(8);

			GroupBox ^ mapGroup = gcnew GroupBox();
			mapGroup->Text = L"Carte";
			mapGroup->Location = Point(8, 8);
			mapGroup->Size = System::Drawing::Size(305, 220);

			mapList = gcnew ComboBox();
			mapList->DropDownStyle = ComboBoxStyle::DropDownList;
			mapList->Location = Point(10, 22);
			mapList->Size = System::Drawing::Size(200, 24);
			mapList->DropDown += gcnew EventHandler(this, &EditorUI::onMapListDropDown);
			openButton = createButton(L"Ouvrir", 216, 21, 80, gcnew EventHandler(this, &EditorUI::onOpen));

			newButton = createButton(L"Nouvelle", 10, 54, 90, gcnew EventHandler(this, &EditorUI::onNew));
			saveButton = createButton(L"Enregistrer", 104, 54, 90, gcnew EventHandler(this, &EditorUI::onSave));
			saveAsButton = createButton(L"Enreg. sous", 198, 54, 98, gcnew EventHandler(this, &EditorUI::onSaveAs));

			nameBox = gcnew TextBox();
			nameBox->Location = Point(60, 92);
			nameBox->Size = System::Drawing::Size(236, 24);
			nameBox->TextChanged += gcnew EventHandler(this, &EditorUI::onMetadataChanged);

			widthBox = gcnew NumericUpDown();
			widthBox->Minimum = Decimal(tw::editor::EditorController::MIN_SIZE);
			widthBox->Maximum = Decimal(tw::editor::EditorController::MAX_SIZE);
			widthBox->Value = Decimal(15);
			widthBox->Location = Point(60, 126);
			widthBox->Size = System::Drawing::Size(55, 24);
			heightBox = gcnew NumericUpDown();
			heightBox->Minimum = Decimal(tw::editor::EditorController::MIN_SIZE);
			heightBox->Maximum = Decimal(tw::editor::EditorController::MAX_SIZE);
			heightBox->Value = Decimal(15);
			heightBox->Location = Point(140, 126);
			heightBox->Size = System::Drawing::Size(55, 24);
			resizeButton = createButton(L"Redimensionner", 200, 124, 96, gcnew EventHandler(this, &EditorUI::onResize));

			poolBox = gcnew CheckBox();
			poolBox->Text = L"Dans le pool des cartes de tournoi";
			poolBox->Location = Point(10, 162);
			poolBox->AutoSize = true;
			poolBox->Checked = true;
			poolBox->CheckedChanged += gcnew EventHandler(this, &EditorUI::onMetadataChanged);

			Label ^ hint = createLabel(L"Nouvelle carte : taille ci-dessus, remplie avec la tuile choisie.", 10, 184);
			hint->ForeColor = Color::DimGray;

			mapGroup->Controls->Add(mapList);
			mapGroup->Controls->Add(openButton);
			mapGroup->Controls->Add(newButton);
			mapGroup->Controls->Add(saveButton);
			mapGroup->Controls->Add(saveAsButton);
			mapGroup->Controls->Add(createLabel(L"Nom :", 10, 92));
			mapGroup->Controls->Add(nameBox);
			mapGroup->Controls->Add(createLabel(L"Taille :", 10, 126));
			mapGroup->Controls->Add(widthBox);
			mapGroup->Controls->Add(createLabel(L"x", 122, 126));
			mapGroup->Controls->Add(heightBox);
			mapGroup->Controls->Add(resizeButton);
			mapGroup->Controls->Add(poolBox);
			mapGroup->Controls->Add(hint);

			GroupBox ^ toolGroup = gcnew GroupBox();
			toolGroup->Text = L"Outils";
			toolGroup->Location = Point(8, 234);
			toolGroup->Size = System::Drawing::Size(305, 164);
			paintTool = createTool(L"Pinceau", 10, 22);
			fillTool = createTool(L"Remplissage", 10, 46);
			rectangleTool = createTool(L"Rectangle", 10, 70);
			team1Tool = createTool(L"Départ équipe 1", 150, 22);
			team2Tool = createTool(L"Départ équipe 2", 150, 46);
			eraseStartTool = createTool(L"Effacer un départ", 150, 70);
			zoneTool = createTool(L"Zone à tenir", 10, 94);
			eraseZoneTool = createTool(L"Effacer la zone", 150, 94);
			undoButton = createButton(L"Annuler (Ctrl+Z)", 10, 124, 140, gcnew EventHandler(this, &EditorUI::onUndo));
			redoButton = createButton(L"Rétablir (Ctrl+Y)", 156, 124, 140, gcnew EventHandler(this, &EditorUI::onRedo));
			toolGroup->Controls->Add(paintTool);
			toolGroup->Controls->Add(fillTool);
			toolGroup->Controls->Add(rectangleTool);
			toolGroup->Controls->Add(team1Tool);
			toolGroup->Controls->Add(team2Tool);
			toolGroup->Controls->Add(eraseStartTool);
			toolGroup->Controls->Add(zoneTool);
			toolGroup->Controls->Add(eraseZoneTool);
			toolGroup->Controls->Add(undoButton);
			toolGroup->Controls->Add(redoButton);

			GroupBox ^ tileGroup = gcnew GroupBox();
			tileGroup->Text = L"Tuiles";
			tileGroup->Location = Point(8, 404);
			tileGroup->Size = System::Drawing::Size(305, 306);
			thumbnails = gcnew ImageList(this->components);
			thumbnails->ImageSize = System::Drawing::Size(64, 64);
			thumbnails->ColorDepth = ColorDepth::Depth32Bit;
			palette = gcnew ListView();
			palette->View = View::LargeIcon;
			palette->LargeImageList = thumbnails;
			palette->MultiSelect = false;
			palette->HideSelection = false;
			palette->Location = Point(10, 22);
			palette->Size = System::Drawing::Size(286, 242);
			palette->SelectedIndexChanged += gcnew EventHandler(this, &EditorUI::onTileSelected);
			reloadTilesButton = createButton(L"Recharger les tuiles", 10, 270, 286, gcnew EventHandler(this, &EditorUI::onReloadTiles));
			tileGroup->Controls->Add(palette);
			tileGroup->Controls->Add(reloadTilesButton);

			GroupBox ^ checkGroup = gcnew GroupBox();
			checkGroup->Text = L"Validation";
			checkGroup->Location = Point(8, 716);
			checkGroup->Size = System::Drawing::Size(305, 170);
			validateButton = createButton(L"Valider la carte", 10, 22, 286, gcnew EventHandler(this, &EditorUI::onValidate));
			validationList = gcnew ListBox();
			validationList->Location = Point(10, 56);
			validationList->Size = System::Drawing::Size(286, 104);
			validationList->HorizontalScrollbar = true;
			validationList->SelectedIndexChanged += gcnew EventHandler(this, &EditorUI::onValidationSelected);
			checkGroup->Controls->Add(validateButton);
			checkGroup->Controls->Add(validationList);

			sidePanel->Controls->Add(mapGroup);
			sidePanel->Controls->Add(toolGroup);
			sidePanel->Controls->Add(tileGroup);
			sidePanel->Controls->Add(checkGroup);

			statusLabel = gcnew Label();
			statusLabel->Dock = DockStyle::Bottom;
			statusLabel->Height = 24;
			statusLabel->TextAlign = ContentAlignment::MiddleLeft;
			statusLabel->Padding = System::Windows::Forms::Padding(6, 0, 0, 0);
			statusLabel->Text = lastMessage;

			this->sfmlRenderingSurface = gcnew SelectablePanel();
			this->sfmlRenderingSurface->Dock = DockStyle::Fill;
			this->sfmlRenderingSurface->Paint += gcnew PaintEventHandler(this, &EditorUI::onSurfacePaint);
			this->sfmlRenderingSurface->GotFocus += gcnew EventHandler(this, &EditorUI::onGotFocus);
			this->sfmlRenderingSurface->LostFocus += gcnew EventHandler(this, &EditorUI::onLostFocus);
			this->sfmlRenderingSurface->Resize += gcnew EventHandler(this, &EditorUI::onSurfaceResize);

			this->renderTimer = gcnew System::Windows::Forms::Timer(this->components);
			this->renderTimer->Interval = 15;
			this->renderTimer->Tick += gcnew EventHandler(this, &EditorUI::onRenderTick);

			this->ClientSize = System::Drawing::Size(1400, 900);
			this->Controls->Add(this->sfmlRenderingSurface);
			this->Controls->Add(sidePanel);
			this->Controls->Add(statusLabel);
			this->Text = L"Éditeur de cartes";
			this->FormClosing += gcnew FormClosingEventHandler(this, &EditorUI::onFormClosing);
			this->ResumeLayout(false);

			paintTool->Checked = true;
		}

		//----------------------------------------------------------
		// Palette
		//----------------------------------------------------------

		Bitmap ^ createThumbnail(const tw::TileDef & tile)
		{
			Bitmap ^ thumbnail = gcnew Bitmap(64, 64);
			Graphics ^ graphics = Graphics::FromImage(thumbnail);
			graphics->Clear(Color::Transparent);
			graphics->InterpolationMode = System::Drawing::Drawing2D::InterpolationMode::HighQualityBicubic;

			String ^ path = fromUtf8(tile.texture);
			if (tile.texture.empty() || !IO::File::Exists(path))
			{
				// Tuile sans texture (trou) : losange en pointillés.
				Pen ^ pen = gcnew Pen(Color::Gray, 2);
				pen->DashStyle = System::Drawing::Drawing2D::DashStyle::Dash;
				array<Point> ^ diamond = { Point(32, 18), Point(62, 33), Point(32, 48), Point(2, 33) };
				graphics->DrawPolygon(pen, diamond);
			}
			else
			{
				Image ^ image = Image::FromFile(path);
				float scale = Math::Min(64.0f / image->Width, 64.0f / image->Height);
				int width = (int)(image->Width * scale);
				int height = (int)(image->Height * scale);
				graphics->DrawImage(image, (64 - width) / 2, 64 - height, width, height);
				delete image;
			}
			delete graphics;
			return thumbnail;
		}

		void fillPalette()
		{
			String ^ selected = palette->SelectedItems->Count > 0 ? (String ^)palette->SelectedItems[0]->Tag : nullptr;
			palette->Items->Clear();
			palette->Groups->Clear();
			thumbnails->Images->Clear();

			Collections::Generic::Dictionary<String ^, ListViewGroup ^> ^ groups = gcnew Collections::Generic::Dictionary<String ^, ListViewGroup ^>();
			for (const tw::TileDef & tile : tw::TileRegistry::get().all())
			{
				String ^ groupName = fromUtf8(tile.group.empty() ? std::string("Autres") : tile.group);
				if (!groups->ContainsKey(groupName))
				{
					ListViewGroup ^ group = gcnew ListViewGroup(groupName);
					groups[groupName] = group;
					palette->Groups->Add(group);
				}

				String ^ id = fromUtf8(tile.id);
				thumbnails->Images->Add(id, createThumbnail(tile));
				ListViewItem ^ item = gcnew ListViewItem(fromUtf8(tile.name), id);
				item->Tag = id;
				item->Group = groups[groupName];
				item->ToolTipText = id;
				palette->Items->Add(item);
				if ((selected != nullptr && String::Equals(selected, id)) || (selected == nullptr && tile.id == controller->getTile()))
					item->Selected = true;
			}
		}

		void onTileSelected(Object ^ sender, EventArgs ^ e)
		{
			if (palette->SelectedItems->Count == 0)
				return;
			controller->setTile(toUtf8((String ^)palette->SelectedItems[0]->Tag));
			// Choisir une tuile ramène au pinceau si un outil de départ était actif.
			if (!paintTool->Checked && !fillTool->Checked && !rectangleTool->Checked)
				paintTool->Checked = true;
		}

		void onReloadTiles(Object ^ sender, EventArgs ^ e)
		{
			std::string error;
			if (!tw::TileRegistry::get().load(tw::TileRegistry::DEFAULT_PATH, &error))
				setStatus(String::Concat(L"Jeu de tuiles non rechargé : ", fromUtf8(error)));
			else
				setStatus(String::Format(L"Jeu de tuiles rechargé ({0} tuiles).", (int)tw::TileRegistry::get().all().size()));
			renderer->reloadTiles();
			fillPalette();
		}

		void onToolChanged(Object ^ sender, EventArgs ^ e)
		{
			if (paintTool->Checked) controller->setTool(tw::editor::Tool::PAINT);
			else if (fillTool->Checked) controller->setTool(tw::editor::Tool::FILL);
			else if (rectangleTool->Checked) controller->setTool(tw::editor::Tool::RECTANGLE);
			else if (team1Tool->Checked) controller->setTool(tw::editor::Tool::START_TEAM1);
			else if (team2Tool->Checked) controller->setTool(tw::editor::Tool::START_TEAM2);
			else if (eraseStartTool->Checked) controller->setTool(tw::editor::Tool::ERASE_START);
			else if (zoneTool->Checked) controller->setTool(tw::editor::Tool::ZONE);
			else if (eraseZoneTool->Checked) controller->setTool(tw::editor::Tool::ERASE_ZONE);
		}

		//----------------------------------------------------------
		// Cartes : liste, nouvelle, ouverture, enregistrement
		//----------------------------------------------------------

		std::vector<int> allMapIds()
		{
			std::vector<int> ids;
			for (const std::string & directory : tw::editor::EditorController::mapDirectories())
			{
				for (int id : tw::EnvironmentManager::getInstance()->getAlreadyExistingIds(directory))
				{
					if (std::find(ids.begin(), ids.end(), id) == ids.end())
						ids.push_back(id);
				}
			}
			std::sort(ids.begin(), ids.end());
			return ids;
		}

		// Dossier de référence pour ouvrir une carte : celui du dépôt s'il existe.
		tw::Environment * loadMapFile(int id)
		{
			std::vector<std::string> directories = tw::editor::EditorController::mapDirectories();
			for (auto it = directories.rbegin(); it != directories.rend(); ++it)
			{
				tw::Environment * environment = tw::EnvironmentManager::getInstance()->loadEnvironmentFrom(*it, id);
				if (environment != NULL)
					return environment;
			}
			return NULL;
		}

		void refreshMapList()
		{
			Object ^ selected = mapList->SelectedItem;
			mapList->Items->Clear();
			for (int id : allMapIds())
			{
				tw::Environment * environment = loadMapFile(id);
				String ^ label = id.ToString();
				if (environment != NULL && !environment->getName().empty())
					label = String::Concat(label, L" - ", fromUtf8(environment->getName()));
				delete environment;
				mapList->Items->Add(label);
			}
			if (selected != nullptr && mapList->Items->Contains(selected))
				mapList->SelectedItem = selected;
		}

		void onMapListDropDown(Object ^ sender, EventArgs ^ e)
		{
			refreshMapList();
		}

		bool confirmDiscard()
		{
			if (!controller->isModified() && !metadataModified)
				return true;

			System::Windows::Forms::DialogResult answer = MessageBox::Show(this,
				L"La carte a été modifiée. Enregistrer les modifications ?", L"Éditeur de cartes",
				MessageBoxButtons::YesNoCancel, MessageBoxIcon::Question);
			if (answer == System::Windows::Forms::DialogResult::Cancel)
				return false;
			if (answer == System::Windows::Forms::DialogResult::Yes)
				return saveMap(false);
			return true;
		}

		void showMetadata()
		{
			tw::Environment * environment = controller->getEnvironment();
			loadingMetadata = true;
			nameBox->Text = fromUtf8(environment->getName());
			poolBox->Checked = environment->isInTournamentPool();
			widthBox->Value = Decimal(environment->getWidth());
			heightBox->Value = Decimal(environment->getHeight());
			loadingMetadata = false;
			metadataModified = false;
			validationList->Items->Clear();
			colorator->setFlagged(-1, -1);
			fitCamera();
		}

		// Toute la carte dans la vue.
		void fitCamera()
		{
			tw::Environment * environment = controller->getEnvironment();
			System::Drawing::Size size = sfmlRenderingSurface->ClientSize;
			camera->fit(environment->getWidth(), environment->getHeight(), sf::Vector2u((unsigned int)size.Width, (unsigned int)size.Height));
		}

		int nextMapId()
		{
			std::vector<int> ids = allMapIds();
			return ids.empty() ? 1 : ids.back() + 1;
		}

		void newMap()
		{
			controller->newMap(Decimal::ToInt32(widthBox->Value), Decimal::ToInt32(heightBox->Value), nextMapId(), controller->getTile());
			showMetadata();
			tw::Environment * environment = controller->getEnvironment();
			setStatus(String::Format(L"Nouvelle carte {0} ({1} x {2}).", environment->getId(), environment->getWidth(), environment->getHeight()));
		}

		void onNew(Object ^ sender, EventArgs ^ e)
		{
			if (confirmDiscard())
				newMap();
		}

		void onOpen(Object ^ sender, EventArgs ^ e)
		{
			if (mapList->SelectedItem == nullptr)
			{
				setStatus(L"Choisissez une carte dans la liste.");
				return;
			}

			String ^ label = mapList->SelectedItem->ToString();
			int id = Int32::Parse(label->Split(' ')[0]);
			if (!confirmDiscard())
				return;

			tw::Environment * environment = loadMapFile(id);
			if (environment == NULL)
			{
				setStatus(String::Format(L"Impossible d'ouvrir la carte {0}.", id));
				return;
			}
			controller->setEnvironment(environment);
			showMetadata();
			setStatus(String::Format(L"Carte {0} ouverte.", id));
		}

		bool saveMap(bool asNew)
		{
			tw::Environment * environment = controller->getEnvironment();
			environment->setName(toUtf8(nameBox->Text->Trim()));
			environment->setInTournamentPool(poolBox->Checked);

			std::vector<tw::editor::ValidationMessage> messages = controller->validate();
			showValidation(messages);
			String ^ problems = L"";
			for (const tw::editor::ValidationMessage & message : messages)
			{
				if (message.blocking)
					problems = String::Concat(problems, L"\n- ", fromUtf8(message.text));
			}
			if (problems->Length > 0)
			{
				System::Windows::Forms::DialogResult answer = MessageBox::Show(this,
					String::Concat(L"La carte n'est pas jouable :", problems, L"\n\nEnregistrer quand même ?"), L"Éditeur de cartes",
					MessageBoxButtons::YesNo, MessageBoxIcon::Warning);
				if (answer != System::Windows::Forms::DialogResult::Yes)
					return false;
			}

			if (asNew)
				environment->setId(nextMapId());

			String ^ savedTo = L"";
			String ^ errors = L"";
			for (const std::string & directory : tw::editor::EditorController::mapDirectories())
			{
				std::string error;
				if (tw::EnvironmentManager::getInstance()->saveEnvironmentTo(environment, directory, &error))
					savedTo = String::Concat(savedTo, savedTo->Length > 0 ? L" et " : L"", fromUtf8(directory));
				else
					errors = String::Concat(errors, L"\n", fromUtf8(error));
			}

			if (errors->Length > 0)
			{
				MessageBox::Show(this, String::Concat(L"Erreur d'enregistrement :", errors), L"Éditeur de cartes", MessageBoxButtons::OK, MessageBoxIcon::Error);
				return false;
			}

			controller->markSaved();
			metadataModified = false;
			refreshMapList();
			setStatus(String::Format(L"Carte {0} enregistrée dans {1}", environment->getId(), savedTo));
			return true;
		}

		void onSave(Object ^ sender, EventArgs ^ e)
		{
			saveMap(false);
		}

		void onSaveAs(Object ^ sender, EventArgs ^ e)
		{
			saveMap(true);
		}

		void onMetadataChanged(Object ^ sender, EventArgs ^ e)
		{
			if (!loadingMetadata)
				metadataModified = true;
		}

		void onResize(Object ^ sender, EventArgs ^ e)
		{
			controller->resize(Decimal::ToInt32(widthBox->Value), Decimal::ToInt32(heightBox->Value), controller->getTile());
			fitCamera();
			setStatus(L"Carte redimensionnée (annulable avec Ctrl+Z).");
		}

		void onFormClosing(Object ^ sender, FormClosingEventArgs ^ e)
		{
			if (!confirmDiscard())
				e->Cancel = true;
		}

		//----------------------------------------------------------
		// Annuler / rétablir, validation
		//----------------------------------------------------------

		void afterHistoryChange()
		{
			tw::Environment * environment = controller->getEnvironment();
			loadingMetadata = true;
			widthBox->Value = Decimal(environment->getWidth());
			heightBox->Value = Decimal(environment->getHeight());
			loadingMetadata = false;
		}

		void undo()
		{
			if (controller->undo())
				afterHistoryChange();
		}

		void redo()
		{
			if (controller->redo())
				afterHistoryChange();
		}

		void onUndo(Object ^ sender, EventArgs ^ e) { undo(); }
		void onRedo(Object ^ sender, EventArgs ^ e) { redo(); }

		void showValidation(const std::vector<tw::editor::ValidationMessage> & messages)
		{
			validationList->Items->Clear();
			colorator->setFlagged(-1, -1);
			if (messages.empty())
			{
				validationList->Items->Add(L"Carte valide : prête à être jouée.");
				return;
			}
			for (const tw::editor::ValidationMessage & message : messages)
			{
				String ^ text = String::Concat(message.blocking ? L"[Bloquant] " : L"[Conseil] ", fromUtf8(message.text));
				if (message.x >= 0)
					text = String::Concat(text, String::Format(L" @{0},{1}", message.x, message.y));
				validationList->Items->Add(text);
			}
		}

		void onValidate(Object ^ sender, EventArgs ^ e)
		{
			showValidation(controller->validate());
		}

		void onValidationSelected(Object ^ sender, EventArgs ^ e)
		{
			// Message avec une case : la vue se centre dessus et la case est surlignée.
			if (validationList->SelectedItem == nullptr)
				return;
			String ^ text = validationList->SelectedItem->ToString();
			int at = text->LastIndexOf(L" @");
			if (at < 0)
				return;
			array<String ^> ^ parts = text->Substring(at + 2)->Split(',');
			int x = Int32::Parse(parts[0]);
			int y = Int32::Parse(parts[1]);
			colorator->setFlagged(x, y);
			camera->centerOn((float)x, (float)y);
		}

		//----------------------------------------------------------
		// Rendu
		//----------------------------------------------------------

		void setStatus(String ^ text)
		{
			lastMessage = text;
			statusLabel->Text = text;
		}

		void onSurfacePaint(Object ^ sender, PaintEventArgs ^ e)
		{
			renderTimer->Enabled = true;
		}

		void onRenderTick(Object ^ sender, EventArgs ^ e)
		{
			float deltatime = renderTimer->Interval / 1000.0f;
			tw::Environment * environment = controller->getEnvironment();

			window->clear(sf::Color(30, 32, 45));
			camera->update(deltatime);
			camera->apply(*renderer);
			renderer->ellapseTime(deltatime);
			renderer->render(environment, *characters, std::vector<tw::AbstractSpellView<sf::Sprite*>*>(), deltatime);
			ticks++;
			if (screenshotPath != nullptr && ticks == 60)
			{
				saveScreenshot();
				window->display();
				Close();
				return;
			}
			window->display();

			undoButton->Enabled = controller->canUndo();
			redoButton->Enabled = controller->canRedo();

			String ^ title = String::Format(L"Éditeur de cartes - Carte {0}", environment->getId());
			if (!environment->getName().empty())
				title = String::Concat(title, L" : ", fromUtf8(environment->getName()));
			title = String::Concat(title, String::Format(L" ({0} x {1})", environment->getWidth(), environment->getHeight()));
			if (controller->isModified() || metadataModified)
				title = String::Concat(title, L" *");
			if (!String::Equals(this->Text, title))
				this->Text = title;

			// Barre d'état : case survolée, puis dernier message.
			String ^ status = lastMessage;
			tw::CellData * cell = environment->getMapData(hoverX, hoverY);
			if (cell != NULL)
			{
				const tw::TileDef * tile = tw::TileRegistry::get().find(cell->getDisplayTile());
				String ^ hover = String::Format(L"Case ({0}, {1}) : {2}", hoverX, hoverY, tile != NULL ? fromUtf8(tile->name) : fromUtf8(cell->getDisplayTile()));
				if (cell->getTeamStartPointNumber() > 0)
					hover = String::Concat(hover, String::Format(L", départ de l'équipe {0}", cell->getTeamStartPointNumber()));
				if (cell->getIsZone())
					hover = String::Concat(hover, L", zone à tenir");
				status = String::Concat(hover, L"     |     ", lastMessage);
			}
			if (!String::Equals(statusLabel->Text, status))
				statusLabel->Text = status;
		}

		// Capture : la fenêtre WinForms, avec le rendu SFML (OpenGL) recopié à l'emplacement de la vue.
		void saveScreenshot()
		{
			sf::Texture capture;
			capture.create(window->getSize().x, window->getSize().y);
			capture.update(*window);
			sf::Image image = capture.copyToImage();

			Bitmap ^ view = gcnew Bitmap((int)image.getSize().x, (int)image.getSize().y, Imaging::PixelFormat::Format32bppArgb);
			for (unsigned int y = 0; y < image.getSize().y; y++)
			{
				for (unsigned int x = 0; x < image.getSize().x; x++)
				{
					sf::Color c = image.getPixel(x, y);
					view->SetPixel((int)x, (int)y, Color::FromArgb(255, c.r, c.g, c.b));
				}
			}

			Bitmap ^ shot = gcnew Bitmap(this->Width, this->Height);
			this->DrawToBitmap(shot, System::Drawing::Rectangle(0, 0, this->Width, this->Height));
			Point origin = sfmlRenderingSurface->PointToScreen(Point::Empty);
			Graphics ^ graphics = Graphics::FromImage(shot);
			graphics->DrawImage(view, origin.X - this->Location.X, origin.Y - this->Location.Y);
			delete graphics;
			shot->Save(screenshotPath, Imaging::ImageFormat::Png);
		}

		void reinitializeRenderer()
		{
			delete window;
			window = new sf::RenderWindow((sf::WindowHandle)sfmlRenderingSurface->Handle.ToPointer());
			renderer->modifyWindow(window);
			window->requestFocus();
		}

		void onSurfaceResize(Object ^ sender, EventArgs ^ e)
		{
			if (window != NULL)
			{
				reinitializeRenderer();
				fitCamera();
			}
		}

		void onGotFocus(Object ^ sender, EventArgs ^ e)
		{
			renderer->forceFocus();
		}

		void onLostFocus(Object ^ sender, EventArgs ^ e)
		{
			renderer->forceUnfocus();
		}
	};
}
