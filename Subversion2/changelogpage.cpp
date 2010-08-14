#include "changelogpage.h"
#include "subversion2.h"

ChangeLogPage::ChangeLogPage(wxWindow* parent, Subversion2* plugin)
	: ChangeLogPageBase(parent)
	, m_plugin(plugin)
{
}

ChangeLogPage::~ChangeLogPage()
{
}

void ChangeLogPage::AppendText(const wxString& text)
{
	m_textCtrl->AppendText(text);
}

void ChangeLogPage::OnURL(wxTextUrlEvent& event)
{
	// we respond only for LeftUp event
	if(event.GetMouseEvent().ButtonUp(wxMOUSE_BTN_LEFT)) {
		int start = event.GetURLStart();
		int end   = event.GetURLEnd();

		wxString url = m_textCtrl->GetRange(start, end);
		wxLaunchDefaultBrowser(url);
	}
}