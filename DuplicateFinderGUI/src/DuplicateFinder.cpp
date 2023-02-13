#include "DuplicateFinder.hpp"

#include "gameengine/IGameEngine.hpp"
#include "gameengine/Camera.hpp"
#include "gameengine/Sprite.hpp"
#include "gameengine/Primitives/Quad.hpp"
#include "gameengine/Cube.hpp"
#include "gameengine/Components/TransformComponent.hpp"

#include "LOGLWAdditionalDeps/ImportImgui.hpp"

#include "CUL/IMPORT_sqlite3.hpp"
#include "CUL/Filesystem/FileFactory.hpp"
#include "CUL/Filesystem/FSApi.hpp"
#include "CUL/GenericUtils/ConsoleUtilities.hpp"
#include "CUL/GenericUtils/ScopeExit.hpp"
#include "CUL/Threading/ThreadUtils.hpp"

#include "CUL/STL_IMPORTS/STD_future.hpp"
#include "CUL/STL_IMPORTS/STD_codecvt.hpp"

#define NFD_NATIVE
#include "nfd.h"


App* App::s_instance = nullptr;

App::App( bool fullscreen, unsigned width, unsigned height, int x, int y, const char* winName, const char* configPath )
    : LOGLW::IGameEngineApp( fullscreen, width, height, x, y, winName, configPath, false )
{
    m_outputFile = "D:\\out.txt";
    m_maxThreadCount = 14;
    m_minFileSizeBytes = 1024 * 2;
}

void App::onInit()
{
    s_instance = this;

    m_culInterface = m_oglw->getCul();

    m_oglw->drawDebugInfo( true );
    m_oglw->setBackgroundColor( LOGLW::ColorE::BLUE );


    const CUL::MATH::Angle pitch( 90.f, CUL::MATH::Angle::Type::DEGREE );

    m_oglw->guiFrameDelegate.addDelegate( [this](){guiIteration();} );

    m_mouseData = m_oglw->getMouseData();

    m_camera = &m_oglw->getCamera();

    m_oglw->setFpsLimit( 60 );
    m_thread.setBody( [this]() {
        timerThread();
    } );

    m_currentFiles.resize( m_maxThreadCount );
    m_thread.run();
}

int App::callback( void*, int argc, char** argv, char** azColName )
{
    m_culInterface->getLogger()->log( "\n\n---------------------------------------------------------------------------------" );
    for( int i = 0; i < argc; ++i )
    {
        char* colName = azColName[i];
        CUL::String logVal = CUL::String( colName ) + CUL::String( " = " ) + CUL::String( argv[i] ? argv[i] : "NULL" );
        m_culInterface->getLogger()->log( logVal );
    }
    m_culInterface->getLogger()->log( "---------------------------------------------------------------------------------\n\n" );
    return 0;
}

void App::onWindowEvent( const SDL2W::WindowEvent::Type e )
{
    if( e == SDL2W::WindowEvent::Type::CLOSE )
    {
        m_runTimer = false;
        close();
    }
}

void App::onKeyBoardEvent( const SDL2W::KeyboardState& keys )
{
    if( keys.at("Q") )
    {
        m_runTimer = false;
        close();
    }
}

void App::timerThread()
{
    while( m_runTimer )
    {
        m_time += 0.01f;
        CUL::ITimer::sleepMiliSeconds(16);
    }
}

void App::customFrame()
{

}

std::string wstring_to_utf8( const std::wstring& str )
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes( str );
}

void App::guiIteration()
{
    auto context = m_oglw->getGuiContext();
    ImGui::SetCurrentContext( context );

    CUL::String name = "MAIN";
    ImGui::Begin( name.cStr(), nullptr,
                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar );

    auto winSize = IGameEngineApp::m_sdlw->getMainWindow()->getSize();

    ImGui::SetWindowPos( { winSize.w * 0.2f, 0 } );

    const auto targetWidht = (float)winSize.w * 0.8f;
    const auto targetHeight = (float)winSize.h * 1.f;

    ImGui::SetWindowSize( { targetWidht, targetHeight } );

    ImGui::Text( "Dirs to check:" );
    if( ImGui::Button( "+" ) )
    {
        addSearchDir();
    }

    ImGui::SameLine();

    if( ImGui::Button( "-" ) )
    {
        removeDir();
    }

    const size_t pathsCount = m_searchPaths.size();
    if( pathsCount > 0 )
    {
        for( size_t i = 0; i < pathsCount; ++i )
        {
            ImGui::Text( m_searchPaths[i].cStr() );
        }
    }

    if( ImGui::Button( "Output file" ) )
    {
        chooseResultFile();
    }

    ImGui::SameLine();

    ImGui::Text( m_outputFile.cStr() );

    {
        std::string textCopy;
        {
            std::lock_guard<std::mutex> guard( m_foundFileMtx );
            {
                textCopy = m_foundFile.string();
            }
        }

        if( false && !textCopy.empty() )
        {
            ImGui::Text( "Found file: " );
            ImGui::SameLine();
            ImGui::Text( textCopy.c_str() );
        }
    }

    {
        std::lock_guard<std::mutex> lock( m_statusMutex );
        if( !m_statusText.empty() )
        {
            ImGui::Text( "Search status: " );
            ImGui::SameLine();
            std::string status = m_statusText.string();
            ImGui::Text( status.c_str() );
        }
    }


    static ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

    if( ImGui::BeginTable( "split", 3, flags ) )
    {
        ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch, 16.f );
        static float width = 270.f;
        ImGui::TableSetupColumn( "Done:", ImGuiTableColumnFlags_WidthStretch, width );
        ImGui::TableSetupColumn( "Current:", ImGuiTableColumnFlags_WidthStretch, width );

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        if( ImGui::Button( "Run" ) )
        {
            if( m_searchThread.joinable() )
            {
                m_searchThread.join();
            }

            m_searchThread = std::thread( &App::searchOneTime, this );
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        if( ImGui::Button( "Run background" ) )
        {
            if( m_searchThread.joinable() )
            {
                m_searchThread.join();
            }

            m_searchThread = std::thread( &App::searchBackground, this );
        }

        CUL::String taskDoneAsString = CUL::String( m_filesDone );
        float percentage = 100.f * ( m_filesDone / m_filesCount );
        if( m_filesCount < 1.f )
        {
            percentage = 0.f;
        }

        if( percentage > 100.f )
        {
            percentage = 100.f;
        }

        const CUL::String percentageAsString = CUL::String( percentage );
        const CUL::String logText = percentageAsString + " done. ";

        ImGui::TableSetColumnIndex( 1 );
        ImGui::Text( logText.cStr(), 0 );

        ImGui::TableSetColumnIndex( 2 );
        ImGui::Text( m_currentFileText.cStr(), 0 );

        ImGui::EndTable();
    }

    if( m_searchStarted )
    {
        for( size_t i = 0; i < m_maxThreadCount; ++i )
        {
            CUL::String currentFileText;
            {
                std::lock_guard<std::mutex> lock( m_workerStatusMtx );
                currentFileText = m_currentFiles[i];
            }

            const auto someString = wstring_to_utf8( currentFileText.getString() );

            ImGui::TextUnformatted( someString.c_str() );
        }
    }


    for( const auto& [fileSizeAsBytes, flieGroup] : m_duplicates )
    {
        const size_t md5Count = flieGroup.MD5Group.size();


        float MegaBytes = 1.f * fileSizeAsBytes / ( 1.f * bytesINMegabyte );

        const CUL::String m_fileSizeString = "Size: " + CUL::String( MegaBytes ) + "MB, MD5 count: " + CUL::String( (int)md5Count );

        bool show = false;
        for( const auto& [md5value, paths] : flieGroup.MD5Group )
        {
            if( paths.files.size() > 1 )
            {
                show = true;
            }
        }

        if( show && ImGui::TreeNode( m_fileSizeString.cStr() ) )
        {
            for( const auto& [md5value, md5Group] : flieGroup.MD5Group )
            {
                const size_t filesCount = md5Group.files.size();
                if( filesCount > 1 )
                {
                    const CUL::String m_md5valueString = "MD5: " + md5value + ", files count: " + CUL::String( (int) filesCount );
                    if( ImGui::TreeNode( m_md5valueString.cStr() ) )
                    {
                        for( const auto& fsPath : md5Group.files )
                        {
                            ImGui::InputText( "default", (char*)fsPath.getPath().cStr(), 64 );
                            //ImGui::BulletText( fsPath.getPath().cStr() );
                        }
                        ImGui::TreePop();
                    }
                }
                
            }
            ImGui::TreePop();
        }
        //
    }

    ImGui::End();
}

void App::addSearchDir()
{
    NFD_Init();

    nfdchar_t* outPath = nullptr;
    nfdfilteritem_t filterItem = {};
    nfdresult_t result = NFD_PickFolder( &outPath, nullptr );
    if( result == NFD_OKAY )
    {
        CUL::String path = outPath;
        m_searchPaths.push_back( path );
        NFD_FreePath( outPath );
    }
    else if( result == NFD_CANCEL )
    {
    }
    else
    {
    }

    NFD_Quit();


    m_logger->log( "found?" );
}

void App::removeDir()
{
    m_searchPaths.pop_back();
}

void App::chooseResultFile()
{
    NFD_Init();

    nfdchar_t* outPath = nullptr;
    nfdfilteritem_t filter = { L"*.txt", L"*.txt" };
    nfdresult_t result = NFD_SaveDialog( &outPath, &filter, 1, nullptr, nullptr );
    if( result == NFD_OKAY )
    {
        m_outputFile = outPath;
        NFD_FreePath( outPath );
    }
    else if( result == NFD_CANCEL )
    {
    }
    else
    {
    }

    NFD_Quit();


    m_logger->log( "found?" );
}

void App::onMouseEvent( const SDL2W::MouseData& mouseData )
{
    static auto windowSize = m_oglw->getMainWindow()->getSize();
    if( true && mouseData.isButtonDown( 1 ) )
    {
        const auto& md = m_oglw->getMouseData();
        auto mouseX = md.getX() - windowSize.getWidth() / 2;
        auto mouseY = md.getY() - windowSize.getHeight() / 2;

        if( m_firstMouse )
        {
            m_mouseLastX = mouseX;
            m_mouseLastY = mouseY;
            m_firstMouse = false;
        }

        int xoffset = mouseX - m_mouseLastX;
        int yoffset = m_mouseLastY - mouseY;  // reversed since y-coordinates go from bottom to top

        static float sensitivity = 0.002f;
        float newX = xoffset * sensitivity;
        float newY = yoffset * sensitivity;

        updateEuler( m_yawLast += newX, m_pitchLast += newY );

        m_mouseLastX = mouseX;
        m_mouseLastY = mouseY;

        m_front = glm::normalize( m_oglw->getCamera().getCenter() );
        m_right = glm::normalize( glm::cross( m_front, m_oglw->getCamera().getUp() ) );
    }
}

void App::updateEuler( float yaw, float pitch )
{
    auto eye = m_oglw->getCamera().getEye();

    CUL::String logVal = "YAW: " + CUL::String( yaw ) + CUL::String( ", PITCH: " ) + CUL::String( pitch );
    m_logger->log( logVal );

    m_lookAngles.Yaw.setValue( yaw, CUL::MATH::Angle::Type::RADIAN );
    m_lookAngles.Pitch.setValue( pitch, CUL::MATH::Angle::Type::RADIAN );

    glm::vec3 NewCenter = moveOnSphere( m_lookAngles.Yaw.getRad(), m_lookAngles.Pitch.getRad(), 0.f, 100.f );
    NewCenter += eye;
    m_oglw->getCamera().setCenter( NewCenter );


    m_yawLast = yaw;
    m_pitchLast = pitch;
}

glm::vec3 App::moveOnSphere( float yaw, float pitch, float /*row*/, float rad )
{
    glm::vec3 result;

    result.x = rad * std::cos( yaw ) * std::cos( pitch );
    result.y = rad * std::sin( pitch );
    result.z = rad * std::sin( yaw ) * std::cos( pitch );

    return result;
}

void App::searchOneTime()
{
    m_culInterface->getLogger()->log( CUL::String( "Start search." ) );
    m_searchStarted = true;

    auto culFF = m_culInterface->getFS();

    startWorkers();

    for( const auto& path : m_searchPaths )
    {
        culFF->ListAllFiles( path, [this] ( const CUL::FS::Path& path ){
            {
                std::lock_guard<std::mutex> guard( m_foundFileMtx );
                m_foundFile = path.getPath();
            }
            m_filesCount = m_filesCount + 1.f;
            addTask( [this, path] ( size_t workerId ){
                m_currentFiles[workerId] = path.getPath();
                addFile( path.getPath().string(), workerId );
                m_currentFiles[workerId] = "";

                m_filesDone = m_filesDone + 1.f;
            } );
        } );
    }

    m_workersEnabled = false;

    for( auto& thread : m_workers )
    {
        if( thread.second.joinable() )
        {
            thread.second.join();
        }
    }

    m_searchStarted = false;

    m_culInterface->getLogger()->log( "\nDONE.");
}

void App::setWorkerStatus( const CUL::String& status, size_t workerId )
{
    std::lock_guard<std::mutex> lock( m_workerStatusMtx );
    m_currentFiles[workerId] = status;
}

void App::searchBackground()
{
    auto culFF = m_culInterface->getFS();
    m_updateDeletedFiles.addTask( [this] (){
        m_culInterface->getLogger()->log( "removeDeletedFilesFromDB::START" );

        setMainStatus( "Loading database..." );
        m_loadingDb = true;
        m_fileDb.loadFilesFromDatabase();
        m_loadingDb = false;
        setMainStatus( "Loading database... done." );
        m_initialDbFilesUpdated = true;

        m_culInterface->getLogger()->log( "removeDeletedFilesFromDB::STOP" );

        m_genericWorker.addTask( [this] (){
            setMainStatus( "Searching files..." );
            searchAllFiles();
        } );
    } );

    m_updateDeletedFiles.setRemoveTasksWhenConsumed( true );
    m_updateDeletedFiles.setThreadName( "m_updateDeletedFiles" );
    m_updateDeletedFiles.SleepMS = 1000;
    m_updateDeletedFiles.run();
    m_runBackground = true;

    m_saveWorker.setRemoveTasksWhenConsumed( false );
    m_saveWorker.setThreadName( "m_saveWorker" );
    m_saveWorker.SleepMS = 4000;
    m_saveWorker.run();
    m_saveWorker.addTask( [this] (){

        if( m_loadingDb )
        {
        }
        else
        {
            m_culInterface->getLogger()->log( "saveDuplicatesToFile::START" );
            saveDuplicatesToFile();
            m_culInterface->getLogger()->log( "saveDuplicatesToFile::STOP" );
        }

        {
            std::lock_guard<std::mutex> lock( m_statusMutex );
            m_statusText = m_fileDb.getDbState();
        }
    } );

    startWorkers();
    m_searchStarted = true;


    m_genericWorker.setThreadName( "m_genericWorker" );
    m_genericWorker.setRemoveTasksWhenConsumed( false );
    m_genericWorker.run();

    while( m_runBackground )
    {
        //std::list<FileGroup>::iterator it;

        //{
        //    std::lock_guard<std::mutex> lock( m_filesMtx );
        //    it = m_files.begin();
        //}

        //while( it != m_files.end() )
        //{
        //    FileGroup& filePath = *it;

        //    for( const auto& file: filePath.files )
        //    {
        //        if( file.exists() )
        //        {
        //            addTask( [this, file] ( size_t workerId ){
        //                m_currentFiles[workerId] = file.getPath();
        //                addFile( file.getPath().string() );
        //                m_currentFiles[workerId] = "";

        //                m_filesDone = m_filesDone + 1.f;
        //            } );
        //        }
        //        else
        //        {
        //            // here add to list for removal.
        //        }
        //    }
        //    std::lock_guard<std::mutex> lock( m_filesMtx );
        //    ++it;
        //}
    }
}

void App::setMainStatus( const CUL::String& status )
{
    std::lock_guard<std::mutex> lock( m_statusMutex );
    m_statusText = status;
}

void App::searchAllFiles()
{
    auto culFF = m_culInterface->getFS();
    for( const auto& m_searchPath : m_searchPaths )
    {
        culFF->ListAllFiles( m_searchPath, [this] ( const CUL::FS::Path& path ){

            if( !path.getIsDir() )
            {
                {
                    std::lock_guard<std::mutex> guard( m_foundFileMtx );
                    m_foundFile = path.getPath();
                }

                addTask( [this, path] ( size_t workerId ){
                    CUL::String pathAsString = path.getPath();
                    addFile( pathAsString, workerId );
                } );
            }
        } );
    }
}

void App::saveDuplicatesToFile()
{
    auto file = m_culInterface->getFF()->createRegularFileRawPtr( CUL::FS::Path( m_outputFile ) );

    CUL::String text = CUL::String( "Files: " );
    m_culInterface->getLogger()->log( text );
    file->addLine( text );

    std::map<FileSize, FileGroup> duplicatesCopy;

    {
        std::lock_guard<std::mutex> lock( m_duplicatesMtx );
        duplicatesCopy = m_duplicates;
    }

    for( const auto& [fileSizeAsBytes, fileGroup] : duplicatesCopy )
    {
        float MegaBytes = 1.f * fileSizeAsBytes / (1.f * bytesINMegabyte );

        text = CUL::String( "Size: " ) + CUL::String( MegaBytes ) + CUL::String("MB");

        file->addLine( text );

        for( const auto& [md5value, md5group] : fileGroup.MD5Group )
        {
            if( md5group.files.size() > 1 )
            {
                text = CUL::String( "MD5: " ) + CUL::String( md5value );
                file->addLine( text );
                for( const auto& fileOfGroup : md5group.files )
                {
                    file->addLine( fileOfGroup );
                }
            }
        }
    }

    file->saveFile();

    delete file;
}

void App::startWorkers()
{
    m_workersEnabled = true;
    for( size_t i = 0; i < m_maxThreadCount; ++i )
    {
        std::thread fileThread( &App::workerThreadMethod, this );
        std::thread::id threadId = fileThread.get_id();
        m_workers[threadId] = std::move( fileThread );
    }
}

void App::workerThreadMethod()
{
    ++m_workersActive;

    const CUL::String threadName = "FILE WORKER " + CUL::String( (int)m_workerId );
    m_oglw->getCul()->getThreadUtils().setCurrentThreadName( threadName );
    const size_t currentWorkerId = m_workerId;
    ++m_workerId;
    
    while( m_workersEnabled || getTasksLeft() > 0 )
    {
        std::function<void( size_t )> task = getTask();

        if( task )
        {
            task( currentWorkerId );
        }
    }
    --m_workersActive;
}


void App::addTask( std::function<void(size_t)> task )
{
    std::lock_guard<std::mutex> functionLock( m_tasksMtx );
    m_tasks.push_back( task );
}

std::function<void(size_t)> App::getTask()
{
    std::function<void( size_t )> task;
    {
        std::lock_guard<std::mutex> functionLock( m_tasksMtx );

        if( m_tasks.size() > 1 )
        {
            task = *m_tasks.rbegin();
            m_tasks.pop_back();
        }
    }

    return task;
}

unsigned App::getTasksLeft()
{
    unsigned result = 0u;
    {
        std::lock_guard<std::mutex> functionLock( m_tasksMtx );
        result = (unsigned) m_tasks.size();
    }
    
    return result;
}

void App::addFile( const CUL::String& path, size_t workerId )
{
    setWorkerStatus( "[START]" + path, workerId );

    std::unique_ptr<CUL::FS::IFile> file;
    file.reset( m_culInterface->getFF()->createRegularFileRawPtr( path ) );
    const auto sizeBytes = file->getSizeBytes().toInt64();
    if( sizeBytes < m_minFileSizeBytes )
    {
        setWorkerStatus( "[File to small, aborting.]" + path, workerId );
        return;
    }

    setWorkerStatus( "[Get last mod time]" + path, workerId );
    const auto modTimeFromFS = file->getLastModificationTime().toString();
    setWorkerStatus( "[Get DB info]" + path, workerId );
    auto info = m_fileDb.getFileInfo( path );
    CUL::String modTimeFromDb;
    if( info.Found )
    {
        modTimeFromDb = info.ModTime;
    }
    else
    {
        setWorkerStatus( "[Get DB info done]" + path, workerId );
    }

    if( modTimeFromFS == modTimeFromDb )
    {
        return;
    }
    else
    {
        setWorkerStatus( "[File changed, calcualte md5...]" + path, workerId );
        auto md5 = file->getMD5();
        setWorkerStatus( "[File changed, calcualte md5 done.]" + path, workerId );

        setWorkerStatus( "[File adding to db...]" + path, workerId );
        m_fileDb.addFile( md5, path, CUL::String( sizeBytes ), modTimeFromFS );
        setWorkerStatus( "[File adding to db... done.]" + path, workerId );
    }
    setWorkerStatus( "[Done.]" + path, workerId );
}

void App::addFileToList( const CUL::String path )
{
}

void App::addDuplicate( const FileSize fileSize, const MD5Value& md5, const CUL::FS::Path& inPath )
{
}

CUL::String App::getModTimeFromDb( const CUL::String& filePath )
{
    auto info = m_fileDb.getFileInfo( filePath );

    if( !info.MD5.empty() )
    {
        return info.FilePath.getLastModificationTime().toString();
    }

    return "";
}

void App::addForCheckForDeletionList( const CUL::String& inPath )
{
    const auto it =  std::find_if( m_deletionList.begin(), m_deletionList.end(), [inPath] ( const CUL::String& path ){
        return path == inPath;
    } );

    if( it == m_deletionList.end() )
    {
        m_deletionList.push_back( inPath );
    }
    else
    {
        auto x = 0;
    }
}

void App::getList()
{
}

void App::printCurrentMean()
{
}

App::~App()
{
    // BYE!
}

int main( int argc, char* args[] )
{
    CUL::GUTILS::ConsoleUtilities cu;
    cu.setArgs( argc, args );
    auto width = cu.getFlagValue( "-w" );
    auto height = cu.getFlagValue( "-h" );

    if( width.empty() || height.empty() )
    {
        width = "1280";
        height = "800";
    }

    App app( false, std::stoul( width.string() ), std::stoul( height.string() ), 256, 127, cu.getArgs().getArgsValVec()[0].cStr(),
             "Config.txt" );
    app.run();

    return 0;
}