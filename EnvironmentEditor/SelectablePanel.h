#pragma once

namespace EnvironmentEditor {
	using namespace System;
	using namespace System::Drawing;
	using namespace System::Windows::Forms;

	ref class SelectablePanel : public Panel {
	public: SelectablePanel(void) : Panel() {
			this->SetStyle(ControlStyles::Selectable, true);
			this->TabStop = true;
		}
	protected: System::Void OnMouseDown(MouseEventArgs ^e) override {
			this->Focus();
			Panel::OnMouseDown(e);
		}
	protected: System::Boolean IsInputKey(Keys keyData) override {
			if (keyData == Keys::Up || keyData == Keys::Down) return true;
			if (keyData == Keys::Left || keyData == Keys::Right) return true;
			return Panel::IsInputKey(keyData);
		}
	protected: System::Void OnEnter(EventArgs ^e) override {
			//this->Invalidate();
			Panel::OnEnter(e);
		}
	protected: System::Void OnLeave(EventArgs ^e) override {
			//this->Invalidate();
			Panel::OnLeave(e);
		}
	protected: System::Void OnPaint(PaintEventArgs ^pe) override {
			Panel::OnPaint(pe);
			if (this->Focused) {
				auto rc = this->ClientRectangle;
				rc.Inflate(-2, -2);
				ControlPaint::DrawFocusRectangle(pe->Graphics, rc);
			}
		}
	};
}
