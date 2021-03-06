#include "mxMainWindow.h"
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/gauge.h>
#include <wx/statline.h>
#include <wx/listbox.h>
#include <wx/app.h>
#include <wx/msgdlg.h>
#include "Package.h"
#include <wx/utils.h>
#include <wx/process.h>
#include <wx/txtstrm.h>
#include "mxSingleCaseWindow.h"
#include <iostream>
#include <wx/settings.h>
#include <wx/button.h>
#include "Application.h"
#include <algorithm>
#include "version.h"

BEGIN_EVENT_TABLE(mxMainWindow,wxFrame)
	EVT_BUTTON(wxID_OK,mxMainWindow::OnButton)
	EVT_END_PROCESS(wxID_ANY,mxMainWindow::OnProcessTerminate)
END_EVENT_TABLE()

mxMainWindow::mxMainWindow ( ) 
	: wxFrame(NULL,wxID_ANY,"PSeInt - Ejercicio",wxDefaultPosition,wxDefaultSize,wxDEFAULT_FRAME_STYLE) 
{
	
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
		
	wxSizerFlags szfc = wxSizerFlags().Center().Border(wxALL,5);
	wxSizerFlags szf = wxSizerFlags().Proportion(0).Expand().Border(wxALL,5);
	wxSizerFlags szfe = szf; szfe.Proportion(1);
	
	sizer = new wxBoxSizer(wxVERTICAL);
	
	results_title = new wxStaticText(this,wxID_ANY,"Cargando ejercicio...");
	sizer->Add(results_title,szf);
	results_bar = new wxGauge(this,wxID_ANY,10,wxDefaultPosition,wxSize(300,15));
	sizer->Add(results_bar,szf);
	
	sizer->Add(new wxStaticLine(this,wxID_ANY),szf);
	
	the_button = new wxButton(this,wxID_OK,"Cancelar");
	sizer->Add(the_button,szfc);

	SetSizerAndFit(sizer);
}

static bool process_finished=true;
static wxProcess *the_process=NULL;
static int process_pid = 0;
static bool abort_test=false;

bool mxMainWindow::RunAllTests(const wxString &cmdline, bool for_create) {

	Show(); wxYield();
	
	wxArrayString tests;
	int number = pack.GetNames(tests);
	if (!number) {
		wxMessageBox("No se encontraron casos de prueba","PSeInt",wxOK|wxICON_ERROR,this);
		return false;
	}
	
	if (!for_create && pack.GetConfigBool("mezclar casos")) {
		std::random_shuffle(tests.begin(),tests.end());
	}
	
	mxSingleCaseWindow *results_win = new mxSingleCaseWindow(this,
		(!for_create)&&pack.GetConfigStr("mostrar casos fallidos")=="primero",
		(!for_create)&&pack.GetConfigBool("mostrar soluciones"));
	
	results_bar->SetRange(number);
	int results_ok=0, results_wrong=0;
	for(int i=0;i<number&&!abort_test;i++) {
		results_title->SetLabel(wxString("Probando ")+tests[i]);
		results_bar->SetValue(i+1);
		Refresh(); wxYield();
		bool ok = RunTest(cmdline,pack.GetTest(tests[i]),for_create);
		if (ok) results_ok++; else results_wrong++;
		if ((!ok)||for_create) {
			results_win->AddCaso(tests[i]);
			if (!for_create && pack.GetConfigStr("mostrar casos fallidos")=="primero") {
				if (wxYES == wxMessageBox(pack.GetConfigStr("mensaje error")
					+"\n\n�Desea ver el primer caso en el que falla?","Resultado",wxYES_NO|wxICON_ERROR,NULL)) 
				{
					results_win->Show();
					return true;
				} else {
					results_win->Destroy();
					return false;
				}
			}
		}
	}
	if (!abort_test) {
		Hide();
		if (for_create||results_wrong) {
			if (for_create||pack.GetConfigStr("mostrar casos fallidos")=="todos") {
				if (for_create||wxYES == wxMessageBox(pack.GetConfigStr("mensaje error")
					+"\n\n�Desea ver los casos en los que falla?","Resultado",wxYES_NO|wxICON_ERROR,NULL)) 
				{
					results_win->Show();
					return true;
				}
			} else {
				wxMessageBox(pack.GetConfigStr("mensaje error"),"Resultado",wxOK|wxICON_ERROR,this);
			}
		} else {
			wxMessageBox(pack.GetConfigStr("mensaje exito"),"Resultado",wxOK|wxICON_EXCLAMATION,this);
		}
	}
	results_win->Destroy();
	return false;
}

bool mxMainWindow::Start (const wxString &fname, const wxString &passkey, const wxString &cmdline) {
	if (!pack.Load(fname,passkey=="--nokey"?"":passkey)) {
		wxMessageBox("Error al cargar el ejercicio","PSeInt",wxOK|wxICON_ERROR,this);
		return false;
	} else {
		
		if (pack.GetConfigInt("version requerida")>PACKAGE_VERSION) {
			wxMessageBox("Debe actualizar PSeInt para poder abrir este ejercicio","Error",wxID_OK|wxICON_ERROR,this);
			return false;
		}
		return RunAllTests(cmdline);
	}
	return false;
}


bool mxMainWindow::RunTest(wxString command, TestCase &test, bool for_create) {
	
	if (for_create) test.solution=test.output="";
	
	the_process  = new wxProcess(this->GetEventHandler());
	the_process->Redirect();
	std::cerr<<"pseval runs: "<<command<<std::endl;
	process_pid = wxExecute(command,wxEXEC_ASYNC,the_process);
	if (!process_pid) return false;
	wxString &input=test.input, &output=test.output, &solution=test.solution;
	wxTextOutputStream in(*the_process->GetOutputStream());
	wxTextInputStream out(*the_process->GetInputStream());
	input.Replace("\r","",true);
	in<<input;
	process_finished=false;
	while(!process_finished) {
		while (the_process->IsInputAvailable()) 
			{ char aux; aux=out.GetChar(); if (aux) output<<aux; }
		wxYield();
	}
	while (the_process->IsInputAvailable())
		{ char aux; aux=out.GetChar(); if (aux) output<<aux; }
	delete the_process; the_process=NULL;
	
	output.Replace("\r","");
	solution.Replace("\r","");
	
	if (for_create) solution=output;
	return output==solution;
	
}

void mxMainWindow::OnProcessTerminate (wxProcessEvent & event) {
	process_finished=true;
}

void mxMainWindow::OnButton (wxCommandEvent & event) {
	if (the_process) {
		if (!process_finished && process_pid) wxKill(process_pid,wxSIGKILL);
		abort_test=true;
	} else {
		wxExit();
	}
}

