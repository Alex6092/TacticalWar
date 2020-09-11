#pragma once

#include <SFML/Graphics.hpp>
#include <IsometricRenderer.h>
#include "EditorEventListener.h"
#include <BaseCharacterModel.h>
#include <Environment.h>
#include <vector>

namespace EnvironmentEditor {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	/// <summary>
	/// Description r�sum�e de EditorUI
	/// </summary>
	public ref class EditorUI : public System::Windows::Forms::Form
	{
		sf::RenderWindow * window;
		tw::IsometricRenderer * renderer;
		EditorEventListener * eventListener;
		tw::Environment * environment;
	private: System::Windows::Forms::Timer^  timer1;
	private: System::Windows::Forms::GroupBox^  actionsGroupBox;

			 std::vector<tw::BaseCharacterModel*> * characters;

			 void reinitializeRenderer()
			 {
				 delete window;
				 delete renderer;
				 
				 window = new sf::RenderWindow((sf::WindowHandle)sfmlRenderingSurface->Handle.ToInt32());
				 renderer = new tw::IsometricRenderer(window);
				 renderer->addEventListener(eventListener);
			 }

	public:
		EditorUI(void)
		{
			InitializeComponent();

			window = new sf::RenderWindow((sf::WindowHandle)sfmlRenderingSurface->Handle.ToInt32());
			renderer = new tw::IsometricRenderer(window);
			environment = new tw::Environment(15, 15);
			environment->getMapData(1, 1)->setIsObstacle(true);
			environment->getMapData(2, 2)->setIsWalkable(false);
			characters = new std::vector<tw::BaseCharacterModel*>();

			eventListener = new EditorEventListener(this);
			renderer->addEventListener(eventListener);
		}

		void editCell(int x, int y)
		{
			environment->getMapData(x, y)->setIsObstacle(true);
		}

	protected:
		/// <summary>
		/// Nettoyage des ressources utilis�es.
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
		/// Variable n�cessaire au concepteur.
		/// </summary>


#pragma region Windows Form Designer generated code
		/// <summary>
		/// M�thode requise pour la prise en charge du concepteur - ne modifiez pas
		/// le contenu de cette m�thode avec l'�diteur de code.
		/// </summary>
		void InitializeComponent(void)
		{
			this->components = (gcnew System::ComponentModel::Container());
			this->sfmlRenderingSurface = (gcnew System::Windows::Forms::Panel());
			this->timer1 = (gcnew System::Windows::Forms::Timer(this->components));
			this->actionsGroupBox = (gcnew System::Windows::Forms::GroupBox());
			this->SuspendLayout();
			// 
			// sfmlRenderingSurface
			// 
			this->sfmlRenderingSurface->Dock = System::Windows::Forms::DockStyle::Fill;
			this->sfmlRenderingSurface->Location = System::Drawing::Point(310, 0);
			this->sfmlRenderingSurface->Name = L"sfmlRenderingSurface";
			this->sfmlRenderingSurface->Size = System::Drawing::Size(964, 613);
			this->sfmlRenderingSurface->TabIndex = 0;
			this->sfmlRenderingSurface->Paint += gcnew System::Windows::Forms::PaintEventHandler(this, &EditorUI::sfmlRenderingSurface_Paint);
			this->sfmlRenderingSurface->Resize += gcnew System::EventHandler(this, &EditorUI::sfmlRenderingSurface_Resize);
			// 
			// timer1
			// 
			this->timer1->Tick += gcnew System::EventHandler(this, &EditorUI::timer1_Tick);
			// 
			// actionsGroupBox
			// 
			this->actionsGroupBox->Dock = System::Windows::Forms::DockStyle::Left;
			this->actionsGroupBox->Location = System::Drawing::Point(0, 0);
			this->actionsGroupBox->Name = L"actionsGroupBox";
			this->actionsGroupBox->Size = System::Drawing::Size(310, 613);
			this->actionsGroupBox->TabIndex = 1;
			this->actionsGroupBox->TabStop = false;
			this->actionsGroupBox->Text = L"Actions :";
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
			this->ResumeLayout(false);

		}
#pragma endregion
	private: System::Void sfmlRenderingSurface_Paint(System::Object^  sender, System::Windows::Forms::PaintEventArgs^  e) {
		timer1->Enabled = true;
	}

	private: System::Void timer1_Tick(System::Object^  sender, System::EventArgs^  e) {
		window->clear();
		renderer->render(environment, *characters);
		window->display();
	}
	private: System::Void sfmlRenderingSurface_Resize(System::Object^  sender, System::EventArgs^  e) {
		reinitializeRenderer();
	}
};
}
