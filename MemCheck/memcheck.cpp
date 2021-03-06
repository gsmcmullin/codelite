/**
 * @file
 * @author pavel.iqx
 * @date 2014
 * @copyright GNU General Public License v2
 */

#include <wx/filedlg.h>
#include <wx/busyinfo.h>

#include "event_notifier.h"
#include "workspace.h"
#include "file_logger.h"
#include "dirsaver.h"
#include "environmentconfig.h"
#include "async_executable_cmd.h"

#include "memcheckdefs.h"
#include "memcheckoutputview.h"
#include "memchecksettingsdialog.h"
#include "valgrindprocessor.h"
#include "memcheckui.h"
#include "memcheck.h"
#include "memchecksettings.h"

static MemCheckPlugin* thePlugin = NULL;

// Define the plugin entry point
extern "C" EXPORT IPlugin* CreatePlugin(IManager* manager)
{
    if(thePlugin == 0) {
        thePlugin = new MemCheckPlugin(manager);
    }
    return thePlugin;
}

extern "C" EXPORT PluginInfo GetPluginInfo()
{
    PluginInfo info;
    info.SetAuthor(wxT("pavel.iqx"));
    info.SetName(wxT("MemCheck"));
    info.SetDescription(wxT("MemCheck plugin detects memory leaks. Uses Valgrind (memcheck tool) as backend."));
    info.SetVersion(wxT("0.4"));
    return info;
}

extern "C" EXPORT int GetPluginInterfaceVersion() { return PLUGIN_INTERFACE_VERSION; }

MemCheckPlugin::MemCheckPlugin(IManager* manager)
    : IPlugin(manager)
    , m_memcheckProcessor(NULL)
    , m_checkProcess(NULL)
{
    // CL_DEBUG1(PLUGIN_PREFIX("MemCheckPlugin constructor"));
    m_longName = wxT("Detects memory management problems. Uses Valgrind - memcheck skin.");
    m_shortName = wxT("MemCheck");

    // menu File and OutputView controls
    m_mgr->GetTheApp()->Connect(XRCID("memcheck_check_active_project"),
                                wxEVT_COMMAND_MENU_SELECTED,
                                wxCommandEventHandler(MemCheckPlugin::OnCheckAtiveProject),
                                NULL,
                                (wxEvtHandler*)this);
    m_mgr->GetTheApp()->Connect(XRCID("memcheck_check_active_project"),
                                wxEVT_UPDATE_UI,
                                wxUpdateUIEventHandler(MemCheckPlugin::OnMemCheckUI),
                                NULL,
                                (wxEvtHandler*)this);

    m_mgr->GetTheApp()->Connect(XRCID("memcheck_check_popup_project"),
                                wxEVT_COMMAND_MENU_SELECTED,
                                wxCommandEventHandler(MemCheckPlugin::OnCheckPopupProject),
                                NULL,
                                (wxEvtHandler*)this);
    m_mgr->GetTheApp()->Connect(XRCID("memcheck_check_popup_project"),
                                wxEVT_UPDATE_UI,
                                wxUpdateUIEventHandler(MemCheckPlugin::OnMemCheckUI),
                                NULL,
                                (wxEvtHandler*)this);

    m_mgr->GetTheApp()->Connect(XRCID("memcheck_check_popup_editor"),
                                wxEVT_COMMAND_MENU_SELECTED,
                                wxCommandEventHandler(MemCheckPlugin::OnCheckPopupEditor),
                                NULL,
                                (wxEvtHandler*)this);
    m_mgr->GetTheApp()->Connect(XRCID("memcheck_check_popup_editor"),
                                wxEVT_UPDATE_UI,
                                wxUpdateUIEventHandler(MemCheckPlugin::OnMemCheckUI),
                                NULL,
                                (wxEvtHandler*)this);

    m_mgr->GetTheApp()->Connect(XRCID("memcheck_import"),
                                wxEVT_COMMAND_MENU_SELECTED,
                                wxCommandEventHandler(MemCheckPlugin::OnImportLog),
                                NULL,
                                (wxEvtHandler*)this);
    m_mgr->GetTheApp()->Connect(XRCID("memcheck_import"),
                                wxEVT_UPDATE_UI,
                                wxUpdateUIEventHandler(MemCheckPlugin::OnMemCheckUI),
                                NULL,
                                (wxEvtHandler*)this);

    m_mgr->GetTheApp()->Connect(XRCID("memcheck_settings"),
                                wxEVT_COMMAND_MENU_SELECTED,
                                wxCommandEventHandler(MemCheckPlugin::OnSettings),
                                NULL,
                                (wxEvtHandler*)this);
    m_mgr->GetTheApp()->Connect(XRCID("memcheck_settings"),
                                wxEVT_UPDATE_UI,
                                wxUpdateUIEventHandler(MemCheckPlugin::OnMemCheckUI),
                                NULL,
                                (wxEvtHandler*)this);

    // EventNotifier::Get()->Connect( wxEVT_INIT_DONE, wxCommandEventHandler(MemCheckPlugin::OnInitDone), NULL, this);
    EventNotifier::Get()->Connect(
        wxEVT_WORKSPACE_LOADED, wxCommandEventHandler(MemCheckPlugin::OnWorkspaceLoaded), NULL, this);
    EventNotifier::Get()->Connect(
        wxEVT_WORKSPACE_CLOSED, wxCommandEventHandler(MemCheckPlugin::OnWorkspaceClosed), NULL, this);

    // CL_DEBUG1(PLUGIN_PREFIX("adding 'Output View' notebook pane"));
    m_outputView = new MemCheckOutputView(m_mgr->GetOutputPaneNotebook(), this, m_mgr);
    m_mgr->GetOutputPaneNotebook()->AddPage(
        m_outputView, wxT("MemCheck"), false, wxXmlResource::Get()->LoadBitmap(wxT("check")));

    m_settings = new MemCheckSettings();
    GetSettings()->LoadFromConfig();
    GetSettings()->SavaToConfig();

    // sets defaults also in OutputView page - important to be here
    ApplySettings();
}

MemCheckPlugin::~MemCheckPlugin()
{
    // CL_DEBUG1(PLUGIN_PREFIX("MemCheckPlugin destroyed"));
    wxDELETE(m_memcheckProcessor);
    wxDELETE(m_settings);
    wxDELETE(m_checkProcess);
}

clToolBar* MemCheckPlugin::CreateToolBar(wxWindow* parent)
{
    // CL_DEBUG1(PLUGIN_PREFIX("MemCheckPlugin::CreateToolBar()"));
    clToolBar* tb(NULL);
    if(m_mgr->AllowToolbar()) {
        int size = m_mgr->GetToolbarIconSize();
        tb = new clToolBar(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, clTB_DEFAULT_STYLE);
        tb->SetToolBitmapSize(wxSize(size, size));
        if(size == 24) {
            tb->AddTool(XRCID("memcheck_check_active_project"),
                        wxT("Run MemCheck"),
                        wxXmlResource::Get()->LoadBitmap(wxT("memcheck_check_24")),
                        wxT("Run MemCheck"),
                        wxITEM_NORMAL);
            tb->AddTool(XRCID("memcheck_import"),
                        wxT("Load MemCheck log from file."),
                        wxXmlResource::Get()->LoadBitmap(wxT("memcheck_import_24")),
                        wxT("Load MemCheck log from file."),
                        wxITEM_NORMAL);
        } else {
            tb->AddTool(XRCID("memcheck_check_active_project"),
                        wxT("Run MemCheck"),
                        wxXmlResource::Get()->LoadBitmap(wxT("memcheck_check")),
                        wxT("Run MemCheck"),
                        wxITEM_NORMAL);
            tb->AddTool(XRCID("memcheck_import"),
                        wxT("Load MemCheck log from file."),
                        wxXmlResource::Get()->LoadBitmap(wxT("memcheck_import")),
                        wxT("Load MemCheck log from file."),
                        wxITEM_NORMAL);
        }
        tb->Realize();
    }
    return tb;
}

void MemCheckPlugin::CreatePluginMenu(wxMenu* pluginsMenu)
{
    // CL_DEBUG1(PLUGIN_PREFIX("MemCheckPlugin::CreatePluginMenu()"));
    wxMenu* menu = new wxMenu();
    wxMenuItem* item(NULL);

    item = new wxMenuItem(
        menu, XRCID("memcheck_check_active_project"), wxT("&Run MemCheck"), wxEmptyString, wxITEM_NORMAL);
    item->SetBitmap(wxXmlResource::Get()->LoadBitmap(wxT("memcheck_check")));
    menu->Append(item);

    item = new wxMenuItem(
        menu, XRCID("memcheck_import"), wxT("&Load MemCheck log from file..."), wxEmptyString, wxITEM_NORMAL);
    item->SetBitmap(wxXmlResource::Get()->LoadBitmap(wxT("memcheck_import")));
    menu->Append(item);

    menu->AppendSeparator();

    item = new wxMenuItem(menu, XRCID("memcheck_settings"), wxT("&Settings..."), wxEmptyString, wxITEM_NORMAL);
    item->SetBitmap(wxXmlResource::Get()->LoadBitmap(wxT("memcheck_settings")));
    menu->Append(item);

    item = new wxMenuItem(pluginsMenu, wxID_ANY, wxT("MemCheck"), wxEmptyString, wxITEM_NORMAL, menu);
    item->SetBitmap(wxXmlResource::Get()->LoadBitmap(wxT("memcheck_check")));
    pluginsMenu->Append(item);
}

void MemCheckPlugin::HookPopupMenu(wxMenu* menu, MenuType type)
{
    if(type == MenuTypeEditor) {
        // items for the editor context menu
        // if(!menu->FindItem(XRCID("memcheck_MenuTypeEditor"))) {
        //     wxMenu* subMenu = new wxMenu();
        //     wxMenuItem* item(NULL);
        // 
        //     item = new wxMenuItem(
        //         subMenu, XRCID("memcheck_check_popup_editor"), wxT("&Run MemCheck"), wxEmptyString, wxITEM_NORMAL);
        //     item->SetBitmap(wxXmlResource::Get()->LoadBitmap(wxT("memcheck_check")));
        //     subMenu->Append(item);
        // 
        //     item = new wxMenuItem(subMenu,
        //                           XRCID("memcheck_import"),
        //                           wxT("&Load MemCheck log from file..."),
        //                           wxEmptyString,
        //                           wxITEM_NORMAL);
        //     item->SetBitmap(wxXmlResource::Get()->LoadBitmap(wxT("memcheck_import")));
        //     subMenu->Append(item);
        // 
        //     subMenu->AppendSeparator();
        // 
        //     item =
        //         new wxMenuItem(subMenu, XRCID("memcheck_settings"), wxT("&Settings..."), wxEmptyString, wxITEM_NORMAL);
        //     item->SetBitmap(wxXmlResource::Get()->LoadBitmap(wxT("memcheck_settings")));
        //     subMenu->Append(item);
        // 
        //     item = new wxMenuItem(
        //         menu, XRCID("memcheck_MenuTypeEditor"), wxT("MemCheck"), wxEmptyString, wxITEM_NORMAL, subMenu);
        //     item->SetBitmap(wxXmlResource::Get()->LoadBitmap(wxT("memcheck_check")));
        //     menu->Append(item);
        // }
    } else if(type == MenuTypeFileExplorer) {
        // items for the file explorer context menu
    } else if(type == MenuTypeFileView_Workspace) {
        // items for the file view / workspace context menu
    } else if(type == MenuTypeFileView_Project) {
        // items for the file view/Project context menu
        if(!menu->FindItem(XRCID("memcheck_MenuTypeFileView_Project"))) {
            wxMenu* subMenu = new wxMenu();
            wxMenuItem* item(NULL);

            item = new wxMenuItem(
                subMenu, XRCID("memcheck_check_popup_project"), wxT("&Run MemCheck"), wxEmptyString, wxITEM_NORMAL);
            item->SetBitmap(wxXmlResource::Get()->LoadBitmap(wxT("memcheck_check")));
            subMenu->Append(item);

            item = new wxMenuItem(subMenu,
                                  XRCID("memcheck_import"),
                                  wxT("&Load MemCheck log from file..."),
                                  wxEmptyString,
                                  wxITEM_NORMAL);
            item->SetBitmap(wxXmlResource::Get()->LoadBitmap(wxT("memcheck_import")));
            subMenu->Append(item);

            subMenu->AppendSeparator();

            item =
                new wxMenuItem(subMenu, XRCID("memcheck_settings"), wxT("&Settings..."), wxEmptyString, wxITEM_NORMAL);
            item->SetBitmap(wxXmlResource::Get()->LoadBitmap(wxT("memcheck_settings")));
            subMenu->Append(item);

            menu->PrependSeparator();
            item = new wxMenuItem(menu,
                                  XRCID("memcheck_MenuTypeFileView_Project"),
                                  wxT("MemCheck"),
                                  wxEmptyString,
                                  wxITEM_NORMAL,
                                  subMenu);
            item->SetBitmap(wxXmlResource::Get()->LoadBitmap(wxT("memcheck_check")));
            menu->Prepend(item);
        }
    } else if(type == MenuTypeFileView_Folder) {
        // items for the file view/Virtual folder context menu
    } else if(type == MenuTypeFileView_File) {
        // items for the file view/file context menu
    }
}

void MemCheckPlugin::UnHookPopupMenu(wxMenu* menu, MenuType type)
{
    if(type == MenuTypeEditor) {
        // items for the editor context menu
    } else if(type == MenuTypeFileExplorer) {
        // items for the file explorer context menu
    } else if(type == MenuTypeFileView_Workspace) {
        // items for the file view / workspace context menu
    } else if(type == MenuTypeFileView_Project) {
        // items for the file view/Project context menu
    } else if(type == MenuTypeFileView_Folder) {
        // items for the file view/Virtual folder context menu
    } else if(type == MenuTypeFileView_File) {
        // items for the file view/file context menu
    }
}

void MemCheckPlugin::UnPlug()
{
    // CL_DEBUG1(PLUGIN_PREFIX("MemCheckPlugin::UnPlug()"));

    m_mgr->GetTheApp()->Disconnect(XRCID("memcheck_check_active_project"),
                                   wxEVT_COMMAND_MENU_SELECTED,
                                   wxCommandEventHandler(MemCheckPlugin::OnCheckAtiveProject),
                                   NULL,
                                   (wxEvtHandler*)this);
    m_mgr->GetTheApp()->Disconnect(XRCID("memcheck_check_active_project"),
                                   wxEVT_UPDATE_UI,
                                   wxUpdateUIEventHandler(MemCheckPlugin::OnMemCheckUI),
                                   NULL,
                                   (wxEvtHandler*)this);

    m_mgr->GetTheApp()->Disconnect(XRCID("memcheck_check_popup_project"),
                                   wxEVT_COMMAND_MENU_SELECTED,
                                   wxCommandEventHandler(MemCheckPlugin::OnCheckPopupProject),
                                   NULL,
                                   (wxEvtHandler*)this);
    m_mgr->GetTheApp()->Disconnect(XRCID("memcheck_check_popup_project"),
                                   wxEVT_UPDATE_UI,
                                   wxUpdateUIEventHandler(MemCheckPlugin::OnMemCheckUI),
                                   NULL,
                                   (wxEvtHandler*)this);

    m_mgr->GetTheApp()->Disconnect(XRCID("memcheck_check_popup_editor"),
                                   wxEVT_COMMAND_MENU_SELECTED,
                                   wxCommandEventHandler(MemCheckPlugin::OnCheckPopupEditor),
                                   NULL,
                                   (wxEvtHandler*)this);
    m_mgr->GetTheApp()->Disconnect(XRCID("memcheck_check_popup_editor"),
                                   wxEVT_UPDATE_UI,
                                   wxUpdateUIEventHandler(MemCheckPlugin::OnMemCheckUI),
                                   NULL,
                                   (wxEvtHandler*)this);

    m_mgr->GetTheApp()->Disconnect(XRCID("memcheck_import"),
                                   wxEVT_COMMAND_MENU_SELECTED,
                                   wxCommandEventHandler(MemCheckPlugin::OnImportLog),
                                   NULL,
                                   (wxEvtHandler*)this);
    m_mgr->GetTheApp()->Disconnect(XRCID("memcheck_import"),
                                   wxEVT_UPDATE_UI,
                                   wxUpdateUIEventHandler(MemCheckPlugin::OnMemCheckUI),
                                   NULL,
                                   (wxEvtHandler*)this);

    m_mgr->GetTheApp()->Disconnect(XRCID("memcheck_settings"),
                                   wxEVT_COMMAND_MENU_SELECTED,
                                   wxCommandEventHandler(MemCheckPlugin::OnSettings),
                                   NULL,
                                   (wxEvtHandler*)this);
    m_mgr->GetTheApp()->Disconnect(XRCID("memcheck_settings"),
                                   wxEVT_UPDATE_UI,
                                   wxUpdateUIEventHandler(MemCheckPlugin::OnMemCheckUI),
                                   NULL,
                                   (wxEvtHandler*)this);

    // EventNotifier::Get()->Disconnect( wxEVT_INIT_DONE, wxCommandEventHandler(MemCheckPlugin::OnInitDone), NULL,
    // this);
    EventNotifier::Get()->Disconnect(
        wxEVT_WORKSPACE_LOADED, wxCommandEventHandler(MemCheckPlugin::OnWorkspaceLoaded), NULL, this);
    EventNotifier::Get()->Disconnect(
        wxEVT_WORKSPACE_CLOSED, wxCommandEventHandler(MemCheckPlugin::OnWorkspaceClosed), NULL, this);

    // before this plugin is un-plugged we must remove the tab we added
    for(size_t i = 0; i < m_mgr->GetOutputPaneNotebook()->GetPageCount(); i++) {
        if(m_outputView == m_mgr->GetOutputPaneNotebook()->GetPage(i)) {
            m_mgr->GetOutputPaneNotebook()->RemovePage(i);
            m_outputView->Destroy();
            break;
        }
    }
}

void MemCheckPlugin::OnWorkspaceLoaded(wxCommandEvent& event)
{
    // CL_DEBUG1(PLUGIN_PREFIX("MemCheckPlugin::OnWorkspaceLoaded()"));
    ApplySettings();
    event.Skip();
}

void MemCheckPlugin::OnWorkspaceClosed(wxCommandEvent& event)
{
    // CL_DEBUG1(PLUGIN_PREFIX("MemCheckPlugin::OnWorkspaceClosed()"));
    ApplySettings();
    event.Skip();
}

bool MemCheckPlugin::IsReady(wxUpdateUIEvent& event)
{
    bool ready = !m_mgr->IsBuildInProgress() && !(m_checkProcess && m_checkProcess->IsBusy());
    int id = event.GetId();
    if(id == XRCID("memcheck_check_active_project")) {
        ready &= !m_mgr->GetWorkspace()->GetActiveProjectName().IsEmpty();
    }
    return ready;
}

void MemCheckPlugin::ApplySettings(bool loadLastErrors)
{
    wxDELETE(m_memcheckProcessor);
    m_memcheckProcessor = new ValgrindMemcheckProcessor(GetSettings());
    if(loadLastErrors) {
        m_outputView->LoadErrors();
        
    } else {
        m_outputView->Clear();
    }
}

void MemCheckPlugin::SwitchToMyPage()
{
    for(size_t i = 0; i < m_mgr->GetOutputPaneNotebook()->GetPageCount(); i++) {
        if(m_outputView == m_mgr->GetOutputPaneNotebook()->GetPage(i)) {
            m_mgr->GetOutputPaneNotebook()->ChangeSelection(i);
            break;
        }
    }
}

void MemCheckPlugin::OnCheckAtiveProject(wxCommandEvent& event)
{
    CHECK_CL_SHUTDOWN()
    Workspace* workspace = m_mgr->GetWorkspace();
    if(workspace) {
        CheckProject(workspace->GetActiveProjectName());
    }
}

void MemCheckPlugin::OnCheckPopupProject(wxCommandEvent& event)
{
    CHECK_CL_SHUTDOWN()
    ProjectPtr project = m_mgr->GetSelectedProject();
    if(project) {
        CheckProject(project->GetName());
    }
}

void MemCheckPlugin::OnCheckPopupEditor(wxCommandEvent& event)
{
    CHECK_CL_SHUTDOWN()
    IEditor* editor = m_mgr->GetActiveEditor();
    if(editor) {
        CheckProject(editor->GetProjectName());
    }
}

void MemCheckPlugin::CheckProject(const wxString& projectName)
{
    if(m_checkProcess && m_checkProcess->IsBusy())
        return; // a process is already running

    wxString errMsg;
    ProjectPtr project = m_mgr->GetWorkspace()->FindProjectByName(projectName, errMsg);
    wxString path = project->GetFileName().GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);

    wxString wd;
    wxString command = m_mgr->GetProjectExecutionCommand(projectName, wd);

    m_checkProcess = new AsyncExeCmd(m_mgr->GetOutputWindow());

    DirSaver ds;
    EnvSetter envGuard(m_mgr->GetEnv());
    wxSetWorkingDirectory(path);
    wxSetWorkingDirectory(wd);

    m_checkProcess->Execute(m_memcheckProcessor->GetExecutionCommand(command), true, true);

    if(m_checkProcess->GetProcess()) {
        m_checkProcess->GetProcess()->Connect(
            wxEVT_END_PROCESS, wxProcessEventHandler(MemCheckPlugin::OnCheckProcessEnd), NULL, this);
    }
}

void MemCheckPlugin::OnCheckProcessEnd(wxProcessEvent& event)
{
    // CL_DEBUG1(PLUGIN_PREFIX("MemCheckPlugin::OnCheckProcessEnd()"));

    m_checkProcess->ProcessEnd(event);
    m_checkProcess->GetProcess()->Disconnect(
        wxEVT_END_PROCESS, wxProcessEventHandler(MemCheckPlugin::OnCheckProcessEnd), NULL, this);
    wxDELETE(m_checkProcess);

    if(event.GetExitCode() == 0) {
        wxWindowDisabler disableAll;
        wxBusyInfo wait(wxT(BUSY_MESSAGE));
        m_mgr->GetTheApp()->Yield();

        m_memcheckProcessor->Process();
        m_outputView->LoadErrors();
        SwitchToMyPage();
    }
}

void MemCheckPlugin::OnImportLog(wxCommandEvent& event)
{
    // CL_DEBUG1(PLUGIN_PREFIX("MemCheckPlugin::OnImportLog()"));

    wxFileDialog openFileDialog(m_mgr->GetTheApp()->GetTopWindow(),
                                wxT("Open log file"),
                                "",
                                "",
                                "xml files (*.xml)|*.xml|all files (*.*)|*.*",
                                wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if(openFileDialog.ShowModal() == wxID_CANCEL)
        return;

    wxWindowDisabler disableAll;
    wxBusyInfo wait(wxT(BUSY_MESSAGE));
    m_mgr->GetTheApp()->Yield();

    if(!m_memcheckProcessor->Process(openFileDialog.GetPath()))
        wxMessageBox(wxT("Output log file cannot be properly loaded."), wxT("Processing error."), wxICON_ERROR);

    m_outputView->LoadErrors();
    SwitchToMyPage();
}

void MemCheckPlugin::OnSettings(wxCommandEvent& event)
{
    // CL_DEBUG1(PLUGIN_PREFIX("MemCheckPlugin::OnSettings()"));
    MemCheckSettingsDialog dialog(m_mgr->GetTheApp()->GetTopWindow(), m_settings);
    if(dialog.ShowModal() == wxID_OK)
        ApplySettings();
}

void MemCheckPlugin::OnMemCheckUI(wxUpdateUIEvent& event)
{
    CHECK_CL_SHUTDOWN()
    event.Enable(IsReady(event));
}
