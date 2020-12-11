#pragma once

#include <SFML/Graphics.hpp>
#include <IsometricRenderer.h>
#include "EditorEventListener.h"
#include <BaseCharacterModel.h>
#include <Environment.h>
#include <vector>
#include <EnvironmentManager.h>
#include <Windows.h>
#include "SelectablePanel.h"
#include "EnvironmentEditorColorator.h"

namespace EnvironmentEditor {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;
	using namespace std;

	/// <summary>
	/// Description résumée de EditorUI
	/// </summary>
	public ref class EditorUI : public System::Windows::Forms::Form
	{
		sf::RenderWindow * window;
		tw::IsometricRenderer * renderer;
		EditorEventListener * eventListener;
		tw::Environment * environment;
	private: System::Windows::Forms::Timer^  timer1;
	private: System::Windows::Forms::GroupBox^  actionsGroupBox;



	private: System::Windows::Forms::GroupBox^  groupBox1;
	private: System::Windows::Forms::RadioButton^  SelectorObstacle;

	private: System::Windows::Forms::RadioButton^  SelectorTrou;

	private: System::Windows::Forms::RadioButton^  SelectorSol;
	private: System::Windows::Forms::RadioButton^  SelectorEquipe2;

	private: System::Windows::Forms::RadioButton^  SelectorEquipe1;

	private: System::Windows::Forms::Button^  button3;
	private: System::Windows::Forms::Button^  button2;
	private: System::Windows::Forms::Button^  button1;
	private: System::Windows::Forms::RadioButton^  SelectorCancelEquipe;
	private: System::Windows::Forms::ComboBox^  mapList;

	private: System::Windows::Forms::Timer^  updateMapListTimer;











			 std::vector<tw::BaseCharacterModel*> * characters;

			 void reinitializeRenderer()
			 {
				 delete window;
				 
				 window = new sf::RenderWindow((sf::WindowHandle)sfmlRenderingSurface->Handle.ToInt32());
				 renderer->modifyWindow(window);
				 renderer->addEventListener(eventListener);
				 window->requestFocus();
			 }

	public:
		EditorUI(void)
		{
			InitializeComponent();

			SelectorSol->Select();

			window = new sf::RenderWindow((sf::WindowHandle)sfmlRenderingSurface->Handle.ToInt32());
			renderer = new tw::IsometricRenderer(window);
			environment = new tw::Environment(15, 15, tw::EnvironmentManager::getInstance()->getAvailableId());

			characters = new std::vector<tw::BaseCharacterModel*>();

			eventListener = new EditorEventListener(this);
			renderer->addEventListener(eventListener);
			renderer->setColorator(new EnvironmentEditorColorator());

			updateMapList();
		}
		//Cellule & Equipe
		void editCell(int x, int y)
		{
			if (SelectorSol->Checked)
			{
				environment->getMapData(x, y)->setIsObstacle(false);
				environment->getMapData(x, y)->setIsWalkable(true);
			}
			else if (SelectorTrou->Checked)
			{
				environment->getMapData(x, y)->setIsObstacle(false);
				environment->getMapData(x, y)->setIsWalkable(false);
			}
			else if (SelectorObstacle->Checked)
			{
				environment->getMapData(x, y)->setIsObstacle(true);
				environment->getMapData(x, y)->setIsWalkable(false);
			}
			else if (SelectorEquipe1->Checked)
			{
				environment->getMapData(x, y)->setTeamStartPoint(1);
			}
			else if (SelectorEquipe2->Checked)
			{
				environment->getMapData(x, y)->setTeamStartPoint(2);
			}
			else if (SelectorCancelEquipe->Checked)
			{
				environment->getMapData(x, y)->setTeamStartPoint(0);
			}
		}

		

	protected:
		/// <summary>
		/// Nettoyage des ressources utilisées.
		/// </summary>
		~EditorUI()
		{
			if (components)
			{
				delete components;
			}
		}
	private: System::Windows::Forms::Panel^  sfmlRenderingSurface;
	private: System::ComponentModel::IContainer^  components;
	protected:

	private:
		/// <summary>
		/// Variable nécessaire au concepteur.
		/// </summary>


#pragma region Windows Form Designer generated code
		/// <summary>
		/// Méthode requise pour la prise en charge du concepteur - ne modifiez pas
		/// le contenu de cette méthode avec l'éditeur de code.
		/// </summary>
		void InitializeComponent(void)
		{
			this->components = (gcnew System::ComponentModel::Container());
			//this->sfmlRenderingSurface = (gcnew System::Windows::Forms::Panel());
			this->sfmlRenderingSurface = (gcnew SelectablePanel());
			this->timer1 = (gcnew System::Windows::Forms::Timer(this->components));
			this->actionsGroupBox = (gcnew System::Windows::Forms::GroupBox());
			this->mapList = (gcnew System::Windows::Forms::ComboBox());
			this->groupBox1 = (gcnew System::Windows::Forms::GroupBox());
			this->SelectorCancelEquipe = (gcnew System::Windows::Forms::RadioButton());
			this->SelectorEquipe2 = (gcnew System::Windows::Forms::RadioButton());
			this->SelectorObstacle = (gcnew System::Windows::Forms::RadioButton());
			this->SelectorEquipe1 = (gcnew System::Windows::Forms::RadioButton());
			this->SelectorTrou = (gcnew System::Windows::Forms::RadioButton());
			this->SelectorSol = (gcnew System::Windows::Forms::RadioButton());
			this->button3 = (gcnew System::Windows::Forms::Button());
			this->button2 = (gcnew System::Windows::Forms::Button());
			this->button1 = (gcnew System::Windows::Forms::Button());
			this->updateMapListTimer = (gcnew System::Windows::Forms::Timer(this->components));
			this->actionsGroupBox->SuspendLayout();
			this->groupBox1->SuspendLayout();
			this->SuspendLayout();
			// 
			// sfmlRenderingSurface
			// 
			this->sfmlRenderingSurface->Dock = System::Windows::Forms::DockStyle::Fill;
			this->sfmlRenderingSurface->Location = System::Drawing::Point(278, 0);
			this->sfmlRenderingSurface->Name = L"sfmlRenderingSurface";
			this->sfmlRenderingSurface->Size = System::Drawing::Size(996, 613);
			this->sfmlRenderingSurface->TabIndex = 0;
			this->sfmlRenderingSurface->Paint += gcnew System::Windows::Forms::PaintEventHandler(this, &EditorUI::sfmlRenderingSurface_Paint);
			this->sfmlRenderingSurface->GotFocus += gcnew System::EventHandler(this, &EditorUI::OnGotFocus);
			this->sfmlRenderingSurface->LostFocus += gcnew System::EventHandler(this, &EditorUI::OnLostFocus);
			this->sfmlRenderingSurface->Resize += gcnew System::EventHandler(this, &EditorUI::sfmlRenderingSurface_Resize);
			// 
			// timer1
			// 
			this->timer1->Interval = 15;
			this->timer1->Tick += gcnew System::EventHandler(this, &EditorUI::timer1_Tick);
			// 
			// actionsGroupBox
			// 
			this->actionsGroupBox->Controls->Add(this->mapList);
			this->actionsGroupBox->Controls->Add(this->groupBox1);
			this->actionsGroupBox->Controls->Add(this->button3);
			this->actionsGroupBox->Controls->Add(this->button2);
			this->actionsGroupBox->Controls->Add(this->button1);
			this->actionsGroupBox->Dock = System::Windows::Forms::DockStyle::Left;
			this->actionsGroupBox->Location = System::Drawing::Point(0, 0);
			this->actionsGroupBox->Name = L"actionsGroupBox";
			this->actionsGroupBox->Size = System::Drawing::Size(278, 613);
			this->actionsGroupBox->TabIndex = 1;
			this->actionsGroupBox->TabStop = false;
			this->actionsGroupBox->Text = L"Actions :";
			this->actionsGroupBox->Enter += gcnew System::EventHandler(this, &EditorUI::actionsGroupBox_Enter);
			// 
			// mapList
			// 
			this->mapList->FormattingEnabled = true;
			this->mapList->Location = System::Drawing::Point(12, 269);
			this->mapList->Name = L"mapList";
			this->mapList->Size = System::Drawing::Size(169, 21);
			this->mapList->TabIndex = 4;
			// 
			// groupBox1
			// 
			this->groupBox1->Controls->Add(this->SelectorCancelEquipe);
			this->groupBox1->Controls->Add(this->SelectorEquipe2);
			this->groupBox1->Controls->Add(this->SelectorObstacle);
			this->groupBox1->Controls->Add(this->SelectorEquipe1);
			this->groupBox1->Controls->Add(this->SelectorTrou);
			this->groupBox1->Controls->Add(this->SelectorSol);
			this->groupBox1->Location = System::Drawing::Point(12, 19);
			this->groupBox1->Name = L"groupBox1";
			this->groupBox1->Size = System::Drawing::Size(250, 200);
			this->groupBox1->TabIndex = 3;
			this->groupBox1->TabStop = false;
			this->groupBox1->Text = L"Type de cellule";
			// 
			// SelectorCancelEquipe
			// 
			this->SelectorCancelEquipe->AutoSize = true;
			this->SelectorCancelEquipe->Location = System::Drawing::Point(25, 166);
			this->SelectorCancelEquipe->Name = L"SelectorCancelEquipe";
			this->SelectorCancelEquipe->Size = System::Drawing::Size(96, 17);
			this->SelectorCancelEquipe->TabIndex = 3;
			this->SelectorCancelEquipe->TabStop = true;
			this->SelectorCancelEquipe->Text = L"Annuler équipe";
			this->SelectorCancelEquipe->UseVisualStyleBackColor = true;
			// 
			// SelectorEquipe2
			// 
			this->SelectorEquipe2->AutoSize = true;
			this->SelectorEquipe2->Location = System::Drawing::Point(25, 143);
			this->SelectorEquipe2->Name = L"SelectorEquipe2";
			this->SelectorEquipe2->Size = System::Drawing::Size(67, 17);
			this->SelectorEquipe2->TabIndex = 1;
			this->SelectorEquipe2->TabStop = true;
			this->SelectorEquipe2->Text = L"Equipe 2";
			this->SelectorEquipe2->UseVisualStyleBackColor = true;
			// 
			// SelectorObstacle
			// 
			this->SelectorObstacle->AutoSize = true;
			this->SelectorObstacle->Location = System::Drawing::Point(25, 79);
			this->SelectorObstacle->Name = L"SelectorObstacle";
			this->SelectorObstacle->Size = System::Drawing::Size(67, 17);
			this->SelectorObstacle->TabIndex = 2;
			this->SelectorObstacle->TabStop = true;
			this->SelectorObstacle->Text = L"Obstacle";
			this->SelectorObstacle->UseVisualStyleBackColor = true;
			// 
			// SelectorEquipe1
			// 
			this->SelectorEquipe1->AutoSize = true;
			this->SelectorEquipe1->Location = System::Drawing::Point(25, 120);
			this->SelectorEquipe1->Name = L"SelectorEquipe1";
			this->SelectorEquipe1->Size = System::Drawing::Size(67, 17);
			this->SelectorEquipe1->TabIndex = 0;
			this->SelectorEquipe1->TabStop = true;
			this->SelectorEquipe1->Text = L"Equipe 1";
			this->SelectorEquipe1->UseVisualStyleBackColor = true;
			// 
			// SelectorTrou
			// 
			this->SelectorTrou->AutoSize = true;
			this->SelectorTrou->Location = System::Drawing::Point(25, 55);
			this->SelectorTrou->Name = L"SelectorTrou";
			this->SelectorTrou->Size = System::Drawing::Size(47, 17);
			this->SelectorTrou->TabIndex = 1;
			this->SelectorTrou->TabStop = true;
			this->SelectorTrou->Text = L"Trou";
			this->SelectorTrou->UseVisualStyleBackColor = true;
			this->SelectorTrou->CheckedChanged += gcnew System::EventHandler(this, &EditorUI::SelectorTrou_CheckedChanged);
			// 
			// SelectorSol
			// 
			this->SelectorSol->AutoSize = true;
			this->SelectorSol->Location = System::Drawing::Point(25, 31);
			this->SelectorSol->Name = L"SelectorSol";
			this->SelectorSol->Size = System::Drawing::Size(40, 17);
			this->SelectorSol->TabIndex = 0;
			this->SelectorSol->TabStop = true;
			this->SelectorSol->Text = L"Sol";
			this->SelectorSol->UseVisualStyleBackColor = true;
			this->SelectorSol->CheckedChanged += gcnew System::EventHandler(this, &EditorUI::SelectorSol_CheckedChanged);
			// 
			// button3
			// 
			this->button3->Location = System::Drawing::Point(12, 225);
			this->button3->Name = L"button3";
			this->button3->Size = System::Drawing::Size(250, 38);
			this->button3->TabIndex = 2;
			this->button3->Text = L"Nouvelle map";
			this->button3->UseVisualStyleBackColor = true;
			this->button3->Click += gcnew System::EventHandler(this, &EditorUI::button3_Click);
			// 
			// button2
			// 
			this->button2->Location = System::Drawing::Point(12, 296);
			this->button2->Name = L"button2";
			this->button2->Size = System::Drawing::Size(250, 38);
			this->button2->TabIndex = 1;
			this->button2->Text = L"Sauvegarde";
			this->button2->UseVisualStyleBackColor = true;
			this->button2->Click += gcnew System::EventHandler(this, &EditorUI::button2_Click);
			// 
			// button1
			// 
			this->button1->Location = System::Drawing::Point(187, 267);
			this->button1->Name = L"button1";
			this->button1->Size = System::Drawing::Size(75, 23);
			this->button1->TabIndex = 0;
			this->button1->Text = L"Chargement";
			this->button1->UseVisualStyleBackColor = true;
			this->button1->Click += gcnew System::EventHandler(this, &EditorUI::button1_Click);
			// 
			// updateMapListTimer
			// 
			this->updateMapListTimer->Enabled = true;
			this->updateMapListTimer->Interval = 1000;
			this->updateMapListTimer->Tick += gcnew System::EventHandler(this, &EditorUI::updateMapListTimer_Tick);
			// 
			// EditorUI
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 13);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(1274, 613);
			this->Controls->Add(this->sfmlRenderingSurface);
			this->Controls->Add(this->actionsGroupBox);
			this->Name = L"EditorUI";
			this->Text = L"EditorUI";
			this->actionsGroupBox->ResumeLayout(false);
			this->groupBox1->ResumeLayout(false);
			this->groupBox1->PerformLayout();
			this->ResumeLayout(false);

		}
#pragma endregion
	private: System::Void sfmlRenderingSurface_Paint(System::Object^  sender, System::Windows::Forms::PaintEventArgs^  e) {
		timer1->Enabled = true;
	}

	private: System::Void timer1_Tick(System::Object^  sender, System::EventArgs^  e) {
		window->clear();
		renderer->render(environment, *characters, 0);
		window->display();
	}
	private: System::Void sfmlRenderingSurface_Resize(System::Object^  sender, System::EventArgs^  e) {
		reinitializeRenderer();
	}

private: System::Void actionsGroupBox_Enter(System::Object^  sender, System::EventArgs^  e) {

}



private: System::Void SelectorSol_CheckedChanged(System::Object^  sender, System::EventArgs^  e) {
}
private: System::Void SelectorTrou_CheckedChanged(System::Object^  sender, System::EventArgs^  e) {
}

private: System::Void button1_Click(System::Object^  sender, System::EventArgs^  e) 
{
	int mapId;
	
	if (mapList->Text->Length > 0 && Int32::TryParse(mapList->Text, mapId))
	{
		tw::Environment * env = tw::EnvironmentManager::getInstance()->loadEnvironment(mapId);
		delete environment;
		this->environment = env;
	}
}

private: System::Void button2_Click(System::Object^  sender, System::EventArgs^  e) 
{
	tw::EnvironmentManager::getInstance()->saveEnvironment(environment);
}


		void OnGotFocus(System::Object ^sender, System::EventArgs ^e)
		{
			renderer->forceFocus();
		}
		void OnLostFocus(System::Object ^sender, System::EventArgs ^e)
		{
			renderer->forceUnfocus();
		}



private: System::Void backgroundWorker1_DoWork(System::Object^  sender, System::ComponentModel::DoWorkEventArgs^  e) {
}
private: System::Void button3_Click(System::Object^  sender, System::EventArgs^  e) {
	int nextId = tw::EnvironmentManager::getInstance()->getAvailableId();
	delete environment;
	this->environment = new tw::Environment(15, 15, nextId);
}
private: System::Void updateMapListTimer_Tick(System::Object^  sender, System::EventArgs^  e) {
	updateMapList();
}

private: 
	void updateMapList()
	{
		if (!mapList->DroppedDown)
		{
			std::vector<int> ids = tw::EnvironmentManager::getInstance()->getAlreadyExistingIds();
			mapList->Items->Clear();

			for (int i = 0; i < ids.size(); i++)
			{
				mapList->Items->Add(ids[i]);
			}
		}
	}
};
}

