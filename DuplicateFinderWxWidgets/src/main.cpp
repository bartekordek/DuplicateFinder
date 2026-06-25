#include "main.hpp"
#include <DuplicateFinderBase.hpp>
#include "CUL/STL_IMPORTS/STD_optional.hpp"

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
    MyFrame(): wxFrame( nullptr, wxID_ANY, "Hello World" )
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

        leftSize( mainSizer );

        //
        // RIGHT PANEL
        //
        {
            wxStaticBoxSizer* rightSizer = new wxStaticBoxSizer( wxVERTICAL, m_mainPanel );

            mainSizer->Add( rightSizer, 1, wxEXPAND | wxALL, 10 );

            m_directoryListWindow = new wxScrolledWindow( rightSizer->GetStaticBox(), wxID_ANY );

            wxBoxSizer* sizer = new wxBoxSizer( wxVERTICAL );

            for( int w = 1; w <= 120; ++w )
            {
                sizer->Add( new wxButton( m_directoryListWindow, wxID_ANY, wxString::Format( "Button %i", w ) ), 0, wxALL | wxEXPAND, 3 );
            }

            m_directoryListWindow->SetSizer( sizer );

            m_directoryListWindow->FitInside();

            m_directoryListWindow->SetScrollRate( 5, 5 );

            rightSizer->Add( m_directoryListWindow, 1, wxEXPAND );
        }

        m_mainPanel->SetSizer( mainSizer );

        addDir( wxString("D:\\") );
    }

    void leftSize( wxSizer* inMainSizer )
    {
        wxStaticBoxSizer* leftSizer = new wxStaticBoxSizer( wxVERTICAL, m_mainPanel );

        wxStaticBox* sizerLeftBox = leftSizer->GetStaticBox();

        leftSizer->Add( new wxStaticText( sizerLeftBox, wxID_ANY, "Dirs to check:" ), 0, wxALIGN_LEFT );

        {
            wxBoxSizer* buttonBox = new wxBoxSizer( wxHORIZONTAL );

            m_addSearchDirectory = new wxButton( sizerLeftBox, wxID_ANY, "+" );

            buttonBox->Add( m_addSearchDirectory, 0 );

            m_addSearchDirectory->Bind( wxEVT_BUTTON, &MyFrame::onAddSearchDirectory, this );

            m_removeSearchDirectory = new wxButton( sizerLeftBox, wxID_ANY, "-" );

            buttonBox->Add( m_removeSearchDirectory, 0 );

            m_removeSearchDirectory->Bind( wxEVT_BUTTON, &MyFrame::onRemoveSearchDirectory, this );

            leftSizer->Add( buttonBox, 0, wxALIGN_LEFT );

            //
            // CUSTOM DIRECTORY LIST
            //
            m_searchDirectoriesListBox = new wxScrolledWindow( sizerLeftBox, wxID_ANY );

            m_searchDirectoriesListBox->SetMinSize( wxSize( 500, 100 ) );

            m_directoriesSizer = new wxBoxSizer( wxVERTICAL );

            m_searchDirectoriesListBox->SetSizer( m_directoriesSizer );

            m_searchDirectoriesListBox->SetScrollRate( 5, 5 );

            leftSizer->Add( m_searchDirectoriesListBox, 1, wxEXPAND | wxALL );
        }

        leftSizer->SetMinSize( 150, 0 );

        inMainSizer->Add( leftSizer, 0, wxGROW | ( wxALL & ~wxLEFT ), 10 );
    }

    void onAddSearchDirectory( wxCommandEvent& )
    {
        if( auto result = openDirectory() )
        {
            m_lastPath = *result;
            addDir( m_lastPath );
        }
        
    }

    void addDir( const wxString& inPath )
    {
        wxPanel* row = new wxPanel( m_searchDirectoriesListBox, wxID_ANY );

        wxBoxSizer* rowSizer = new wxBoxSizer( wxHORIZONTAL );

        wxButton* browseBtn = new wxButton( row, wxID_ANY, "" );
        browseBtn->SetBitmap( wxArtProvider::GetBitmap( wxART_FOLDER_OPEN, wxART_BUTTON ) );

        wxButton* deleteBtn = new wxButton( row, wxID_ANY, "" );
        deleteBtn->SetBitmap( wxArtProvider::GetBitmap( wxART_DELETE, wxART_BUTTON ) );

        wxStaticText* dirLabel = new wxStaticText( row, wxID_ANY, inPath );

        rowSizer->Add( browseBtn, 0, wxALL, 4 );
        rowSizer->Add( deleteBtn, 0, wxALL, 4 );
        rowSizer->Add( dirLabel, 1, wxALIGN_CENTER_VERTICAL | wxALL, 4 );

        row->SetSizer( rowSizer );

        m_directoriesSizer->Add( row, 0, wxEXPAND );

        m_searchDirectoriesListBox->Layout();
        m_searchDirectoriesListBox->FitInside();

        browseBtn->Bind( wxEVT_BUTTON,
                         [this, dirLabel]( wxCommandEvent& )
                         {
                             if( auto result = openDirectory() )
                             {
                                 m_lastPath = *result;
                                 dirLabel->SetLabel( m_lastPath );
                             }
                         } );
    }

    void onRemoveSearchDirectory( wxCommandEvent& )
    {
    }

private:
    void onBrowseButtonClicked( wxCommandEvent& inOutEvent )
    {

    }

    std::optional<wxString> openDirectory()
    {
        std::optional<wxString> result;

        constexpr int style = wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST;

        wxDirDialog dlg( this, "Choose Directory", wxEmptyString, style );

        if( dlg.ShowModal() == wxID_OK )
        {
            result = dlg.GetPath();
        }

        return result;
    }

    void OnHello( wxCommandEvent& )
    {
    }

    void OnExit( wxCommandEvent& )
    {
        Close( true );
    }

    void OnAbout( wxCommandEvent& )
    {
        wxMessageBox( "This is a wxWidgets Hello World example", "About", wxOK | wxICON_INFORMATION );
    }

    DuplicateFinderBase m_duplicateFinderBase;

    wxString m_lastPath;

    wxPanel* m_mainPanel{ nullptr };

    wxButton* m_addSearchDirectory{ nullptr };
    wxButton* m_removeSearchDirectory{ nullptr };

    wxScrolledWindow* m_searchDirectoriesListBox{ nullptr };

    wxScrolledWindow* m_directoryListWindow{ nullptr };

    wxBoxSizer* m_directoriesSizer{ nullptr };

    CUL_NONCOPYABLE( MyFrame )
};

bool MyApp::OnInit()
{
    MyFrame* frame = new MyFrame();
    frame->SetSize( wxSize( 1200, 800 ) );
    frame->Show();
    return true;
}

wxIMPLEMENT_APP_CONSOLE( MyApp );