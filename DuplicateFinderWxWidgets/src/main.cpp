#include "main.hpp"
#include <DuplicateFinderBase.hpp>

enum
{
    ID_Hello = 1
};

MyApp::MyApp()
{
}

class MyFrame final: public wxFrame
{
public:
    MyFrame() : wxFrame( nullptr, wxID_ANY, "Hello World" )
    {
        wxMenu* menuFile = new wxMenu;
        menuFile->Append( ID_Hello, "&Hello...\tCtrl-H", "Help string shown in status bar for this menu item" );
        menuFile->AppendSeparator();
        menuFile->Append( wxID_EXIT );

        wxMenu* menuHelp = new wxMenu;
        menuHelp->Append( wxID_ABOUT );

        wxMenuBar* menuBar = new wxMenuBar;
        menuBar->Append( menuFile, "&File" );
        menuBar->Append( menuHelp, "&Help" );

        SetMenuBar( menuBar );

        CreateStatusBar();
        SetStatusText( "Welcome to wxWidgets!" );

        Bind( wxEVT_MENU, &MyFrame::OnHello, this, ID_Hello );
        Bind( wxEVT_MENU, &MyFrame::OnAbout, this, wxID_ABOUT );
        Bind( wxEVT_MENU, &MyFrame::OnExit, this, wxID_EXIT );

        m_mainPanel = new wxPanel( this, wxID_ANY );

        wxSizer* mainSizer = new wxBoxSizer( wxHORIZONTAL );

        {
            wxStaticBoxSizer* leftSizer = new wxStaticBoxSizer( wxVERTICAL, m_mainPanel );
            wxStaticBox* const sizerLeftBox = leftSizer->GetStaticBox();
            {
                wxStaticText* dirsToCheckDir = new wxStaticText( sizerLeftBox, wxID_ANY, "Dirs to check:" );
                leftSizer->Add( dirsToCheckDir, 0, wxALIGN_LEFT );
            }

            {
                wxBoxSizer* button_box = new wxBoxSizer( wxHORIZONTAL );
                m_addSearchDirectory = new wxButton( sizerLeftBox, wxID_ANY, "+" );
                button_box->Add( m_addSearchDirectory, 0 );
                m_addSearchDirectory->Bind( wxEVT_BUTTON, &MyFrame::onAddSearchDirectory, this );

                m_removeSearchDirectory = new wxButton( sizerLeftBox, wxID_ANY, "-" );
                button_box->Add( m_removeSearchDirectory, 0 );
                m_removeSearchDirectory->Bind( wxEVT_BUTTON, &MyFrame::onRemoveSearchDirectory, this );
                leftSizer->Add( button_box, 0, wxALIGN_LEFT );

                m_searchDirectoriesListBox = new wxListBox( sizerLeftBox, wxID_ANY );
                m_searchDirectoriesListBox->SetMinSize(wxSize(500, 100));
                leftSizer->Add( m_searchDirectoriesListBox, 0, wxEXPAND | wxALL );
            }

            leftSizer->SetMinSize( 150, 0 );

            mainSizer->Add( leftSizer, 0, wxGROW | ( wxALL & ~wxLEFT ), 10 );
        }
        {
            wxStaticBoxSizer* rightSizer = new wxStaticBoxSizer( wxVERTICAL, m_mainPanel );
            mainSizer->Add( rightSizer, 1, wxEXPAND | wxALL, 10 );

            m_directoryListWindow = new wxScrolledWindow( rightSizer->GetStaticBox(), wxID_ANY );
            wxBoxSizer* sizer = new wxBoxSizer( wxVERTICAL );
            for( int w = 1; w <= 120; w++ )
            {
                wxButton* button = new wxButton( m_directoryListWindow, wxID_ANY, wxString::Format( "Button %i", w ) );
                sizer->Add( button, 0, wxALL | wxEXPAND, 3 );
            }
            m_directoryListWindow->SetSizer( sizer );
            m_directoryListWindow->FitInside();
            m_directoryListWindow->SetScrollRate( 5, 5 );
            rightSizer->Add( m_directoryListWindow, 1, wxEXPAND );
        }
        m_mainPanel->SetSizer( mainSizer );

        addDir( "ddddddddddddddd" );
    }

    void onAddSearchDirectory( wxCommandEvent& inCommandEvent )
    {
        constexpr int style = wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST;
        wxDirDialog dlg( this, "Choose File", wxEmptyString, style );

        if( dlg.ShowModal() == wxID_OK )
        {
            m_lastPath = dlg.GetPath();
            const wxCStrData stringData = m_lastPath.c_str();
#if CUL_USE_WCHAR
            m_duplicateFinderBase.addPath( stringData.AsWChar() );
#else   // CUL_USE_WCHAR
            m_duplicateFinderBase.addPath( stringData.AsChar() );
#endif  // CUL_USE_WCHAR

            addDir( dlg.GetPath() );
        }
    }

    void addDir( const wxString& inPath )
    {
        wxStaticBoxSizer* dirListSizer = new wxStaticBoxSizer( wxHORIZONTAL, m_searchDirectoriesListBox );

        wxStaticText* dirLabel = new wxStaticText( dirListSizer->GetStaticBox(), wxID_ANY, inPath );

        dirListSizer->Add( dirLabel, 1, wxEXPAND | wxALL, 4 );

        m_searchDirectoriesListBox->GetSizer()->Add( dirListSizer, 0, wxEXPAND );

        m_searchDirectoriesListBox->Layout();
    }

    void onRemoveSearchDirectory( wxCommandEvent& inCommandEvent )
    {
    }

    ~MyFrame()
    {
    }

private:
    DuplicateFinderBase m_duplicateFinderBase;
    wxString m_lastPath;
    void OnHello( wxCommandEvent& event )
    {
    }

    void OnExit( wxCommandEvent& event )
    {
        Close( true );
    }
    void OnAbout( wxCommandEvent& event )
    {
        wxMessageBox( "This is a wxWidgets Hello World example", "About Hello World", wxOK | wxICON_INFORMATION );
    }

    wxPanel* m_mainPanel
    {
    nullptr};
    wxButton* m_addSearchDirectory{ nullptr };
    wxButton* m_removeSearchDirectory{ nullptr };
    wxScrolledWindow* m_searchDirectoriesListBox{ nullptr };
    wxScrolledWindow* m_directoryListWindow{nullptr};
    wxBoxSizer* m_directories_sizer{ nullptr };
    wxPanel* m_directories_panel{ nullptr };

    CUL_NONCOPYABLE( MyFrame )
};


bool MyApp::OnInit()
{
    MyFrame* frame = new MyFrame();
    frame->SetSize(wxSize(1200, 800));
    frame->Show();
    return true;
}

// wxIMPLEMENT_APP( MyApp );
wxIMPLEMENT_APP_CONSOLE( MyApp );