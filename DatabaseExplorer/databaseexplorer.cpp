#include <wx/aboutdlg.h>
#include "databaseexplorer.h"
#include "detachedpanesinfo.h"
#include "dockablepane.h"
#include <wx/xrc/xmlres.h>
#include "wx/wxsf/AutoLayout.h"

static DatabaseExplorer* thePlugin = NULL;

IManager* DatabaseExplorer::GetManager()
{
	return thePlugin->m_mgr;
}

enum wxbuildinfoformat {
    short_f, long_f };

wxString wxbuildinfo(wxbuildinfoformat format)
{
    wxString wxbuild(wxVERSION_STRING);

    if (format == long_f )
    {
#if defined(__WXMSW__)
        wxbuild << _T("-Windows");
#elif defined(__WXMAC__)
        wxbuild << _T("-Mac");
#elif defined(__UNIX__)
        wxbuild << _T("-Linux");
#endif

#if wxUSE_UNICODE
        wxbuild << _T("-Unicode build");
#else
        wxbuild << _T("-ANSI build");
#endif // wxUSE_UNICODE
    }

    return wxbuild;
}

//Define the plugin entry point
extern "C" EXPORT IPlugin *CreatePlugin(IManager *manager)
{
	if (thePlugin == 0) {
		thePlugin = new DatabaseExplorer(manager);
	}
	return thePlugin;
}

extern "C" EXPORT PluginInfo GetPluginInfo()
{
	PluginInfo info;
	info.SetAuthor(wxT("Peter Janků, Michal Bližňák, Tomas Bata University in Zlin, Czech Republic (www.fai.utb.cz)"));
	info.SetName(wxT("DatabaseExplorer"));
	info.SetDescription(wxT("DatabaseExplorer for CodeLite"));
	info.SetVersion(wxT("v1.0"));
	return info;
}

extern "C" EXPORT int GetPluginInterfaceVersion()
{
	return PLUGIN_INTERFACE_VERSION;
}

DatabaseExplorer::DatabaseExplorer(IManager *manager)
		: IPlugin(manager)
{
	
	// create tab (possibly detached)
	Notebook *book = m_mgr->GetWorkspacePaneNotebook();
	wxWindow *editorBook = m_mgr->GetEditorPaneNotebook();
	
	
	if( IsDbViewDetached() ) {
		DockablePane *cp = new DockablePane(book->GetParent()->GetParent(), book, wxT("DbExplorer"), wxNullBitmap, wxSize(200, 200));
		m_dbViewerPanel = new DbViewerPanel(cp, editorBook, m_mgr  );
		cp->SetChildNoReparent(m_dbViewerPanel);

	} else {

		m_dbViewerPanel = new DbViewerPanel(book, editorBook, m_mgr );
		//size_t index = GetSettings().GetSvnTabIndex();
		//if(index == Notebook::npos)
			book->AddPage(m_dbViewerPanel,  wxT("DbExplorer"), false);
		//else
		//	book->InsertPage(index, m_dbViewerPanel, svnCONSOLE_TEXT, false);
	}
	
	// configure autolayout algorithns
	wxSFAutoLayout layout;
	
	wxSFLayoutHorizontalTree *pHTreeAlg = wxDynamicCast( layout.GetAlgorithm( wxT("Horizontal Tree") ), wxSFLayoutHorizontalTree );
	if( pHTreeAlg ) pHTreeAlg->SetHSpace( 200 );
	
	wxSFLayoutVerticalTree *pVTreeAlg = wxDynamicCast( layout.GetAlgorithm( wxT("Vertical Tree") ), wxSFLayoutVerticalTree );
	if( pVTreeAlg ) pVTreeAlg->SetVSpace( 75 );
	
	
	m_longName = wxT("DatabaseExplorer for CodeLite");
	m_shortName = wxT("DatabaseExplorer");
}

DatabaseExplorer::~DatabaseExplorer()
{
	wxSFAutoLayout::CleanUp();
}

clToolBar *DatabaseExplorer::CreateToolBar(wxWindow *parent)
{
	// Create the toolbar to be used by the plugin
	clToolBar *tb(NULL);
	
/*
	// You can use the below code a snippet:
 	// First, check that CodeLite allows plugin to register plugins
	if (m_mgr->AllowToolbar()) {
		// Support both toolbars icon size
		int size = m_mgr->GetToolbarIconSize();
		
		// Allocate new toolbar, which will be freed later by CodeLite
		tb = new clToolBar(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, clTB_DEFAULT_STYLE);
		
		// Set the toolbar size
		tb->SetToolBitmapSize(wxSize(size, size));
		
		// Add tools to the plugins toolbar. You must provide 2 sets of icons: 24x24 and 16x16
		if (size == 24) {
			tb->AddTool(XRCID("new_plugin"), wxT("New CodeLite Plugin Project"), wxXmlResource::Get()->LoadBitmap(wxT("plugin24")), wxT("New Plugin Wizard..."));
			tb->AddTool(XRCID("new_class"), wxT("Create New Class"), wxXmlResource::Get()->LoadBitmap(wxT("class24")), wxT("New Class..."));
			tb->AddTool(XRCID("new_wx_project"), wxT("New wxWidget Project"), wxXmlResource::Get()->LoadBitmap(wxT("new_wx_project24")), wxT("New wxWidget Project"));
		} else {
			tb->AddTool(XRCID("new_plugin"), wxT("New CodeLite Plugin Project"), wxXmlResource::Get()->LoadBitmap(wxT("plugin16")), wxT("New Plugin Wizard..."));
			tb->AddTool(XRCID("new_class"), wxT("Create New Class"), wxXmlResource::Get()->LoadBitmap(wxT("class16")), wxT("New Class..."));
			tb->AddTool(XRCID("new_wx_project"), wxT("New wxWidget Project"), wxXmlResource::Get()->LoadBitmap(wxT("new_wx_project16")), wxT("New wxWidget Project"));
		}
		// And finally, we must call 'Realize()'
		tb->Realize();
	}
	
	// return the toolbar, it can be NULL if CodeLite does not allow plugins to register toolbars
	// or in case the plugin simply does not require toolbar
*/
	return tb;
}

void DatabaseExplorer::CreatePluginMenu(wxMenu *pluginsMenu)
{
	
	// You can use the below code a snippet:
 	wxMenu *menu = new wxMenu();
	wxMenuItem *item(NULL);
	item = new wxMenuItem(menu, XRCID("dbe_about"), _("About dialog..."), wxEmptyString, wxITEM_NORMAL);
	menu->Append(item);
	//item = new wxMenuItem(menu, XRCID("new_class"), _("New Class Wizard..."), wxEmptyString, wxITEM_NORMAL);
	//menu->Append(item);
	//item = new wxMenuItem(menu, XRCID("new_wx_project"), _("New wxWidgets Project Wizard..."), wxEmptyString, wxITEM_NORMAL);
	//menu->Append(item);
	pluginsMenu->Append(wxID_ANY, _("Database Explorer"), menu);
	m_mgr->GetTheApp()->Connect( XRCID("dbe_about"), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( DatabaseExplorer::OnAbout ), NULL, this );


}

void DatabaseExplorer::HookPopupMenu(wxMenu *menu, MenuType type)
{
	if (type == MenuTypeEditor) {
		//TODO::Append items for the editor context menu
	} else if (type == MenuTypeFileExplorer) {
		//TODO::Append items for the file explorer context menu
	} else if (type == MenuTypeFileView_Workspace) {
		//TODO::Append items for the file view / workspace context menu
	} else if (type == MenuTypeFileView_Project) {
		//TODO::Append items for the file view/Project context menu
	} else if (type == MenuTypeFileView_Folder) {
		//TODO::Append items for the file view/Virtual folder context menu
	} else if (type == MenuTypeFileView_File) {
		//TODO::Append items for the file view/file context menu
	}
}

void DatabaseExplorer::UnHookPopupMenu(wxMenu *menu, MenuType type)
{
	if (type == MenuTypeEditor) {
		//TODO::Unhook items for the editor context menu
	} else if (type == MenuTypeFileExplorer) {
		//TODO::Unhook  items for the file explorer context menu
	} else if (type == MenuTypeFileView_Workspace) {
		//TODO::Unhook  items for the file view / workspace context menu
	} else if (type == MenuTypeFileView_Project) {
		//TODO::Unhook  items for the file view/Project context menu
	} else if (type == MenuTypeFileView_Folder) {
		//TODO::Unhook  items for the file view/Virtual folder context menu
	} else if (type == MenuTypeFileView_File) {
		//TODO::Unhook  items for the file view/file context menu
	}
}

void DatabaseExplorer::UnPlug()
{
	//TODO:: perform the unplug action for this plugin
	
	
	int index = m_mgr->GetWorkspacePaneNotebook()->GetPageIndex(m_dbViewerPanel);
	if (index != Notebook::npos) {
		m_mgr->GetWorkspacePaneNotebook()->RemovePage(index, false);
	}
	delete m_dbViewerPanel;
	m_dbViewerPanel = NULL;
	
	
}


bool DatabaseExplorer::IsDbViewDetached()
{
	DetachedPanesInfo dpi;
	m_mgr->GetConfigTool()->ReadObject(wxT("DetachedPanesList"), &dpi);
	wxArrayString detachedPanes = dpi.GetPanes();
	
	
	return detachedPanes.Index(wxT("DbExplorer")) != wxNOT_FOUND;
	
}


void DatabaseExplorer::OnAbout(wxCommandEvent& e)
{
	wxString version = wxString::Format( wxT("0.1.0.1p Alpha"));

    wxString desc = wxT("Cross-platform database plugin designed for managing data, ERD and code generation.\n\n");
	desc << wxbuildinfo(long_f) << wxT("\n\n");

    wxAboutDialogInfo info;
    info.SetName(wxT("DatabaseExplorer"));
    info.SetVersion(version);
    info.SetDescription(desc);
    info.SetCopyright(wxT("2011 (C) Tomas Bata University, Zlin, Czech Republic"));
    info.SetWebSite(wxT("http://www.fai.utb.cz"));
    info.AddDeveloper(wxT("Peter Janků"));
    info.AddDeveloper(wxT("Michal Bližňák"));

    wxAboutBox(info);
	
}
