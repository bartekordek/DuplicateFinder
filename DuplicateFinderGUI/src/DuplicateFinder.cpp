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
#include "CUL/Threading/ThreadUtils.hpp"

#include "CUL/STL_IMPORTS/STD_future.hpp"
#include "CUL/STL_IMPORTS/STD_codecvt.hpp"

#define NFD_NATIVE
#include "nfd.h"


App* App::s_instance = nullptr;

App::App( bool fullscreen, unsigned width, unsigned height, int x, int y, const char* winName, const char* configPath )
    : LOGLW::IGameEngineApp( fullscreen, width, height, x, y, winName, configPath, false )
{
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

    initDb();

    m_currentFiles.resize( m_maxThreadCount );
    m_thread.run();
}

void App::initDb()
{
    int rc = sqlite3_open( "FilesList.db", &m_db );

    std::string sqlQuery =
        "SELECT * \
    FROM FILES";

    auto callback = []( void*, int, char**, char** )
    {
        // DuplicateFinder::s_instance->callback(NotUsed, argc, argv, azColName);
        return 0;
    };

    char* zErrMsg = nullptr;
    rc = sqlite3_exec( m_db, sqlQuery.c_str(), nullptr, 0, &zErrMsg );

    if( rc != SQLITE_OK )
    {
        sqlQuery =
            "CREATE TABLE FILES (\
        PATH varchar(1024),\
        SIZE varchar(512),\
        MD5 varchar(1024),\
        LAST_MODIFICATION varchar(1024)\
        );";
        std::string errorResult( zErrMsg );
        rc = sqlite3_exec( m_db, sqlQuery.c_str(), callback, 0, &zErrMsg );
    }
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

    if( !m_foundFile.empty() )
    {
        ImGui::Text( "Found file: " );
        ImGui::SameLine();
        ImGui::Text( m_foundFile.cStr() );
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
            const CUL::String currentFileText = "CURRENT FILE ON " + CUL::String( (int)i ) + CUL::String( " " ) + m_currentFiles[i];

            const auto someString = wstring_to_utf8( currentFileText.getString() );

            ImGui::TextUnformatted( someString.c_str() );
        }
    }


    for( const auto& [filesize, value] : m_duplicates )
    {
        const size_t md5Count = value.size();
        const CUL::String m_fileSizeString = "Size: " + CUL::String( filesize ) + ", MD5 count: " + CUL::String( (int)md5Count );

        bool show = false;
        for( const auto& [md5value, paths] : value )
        {
            if( paths.size() > 1 )
            {
                show = true;
            }
        }

        if( show && ImGui::TreeNode( m_fileSizeString.cStr() ) )
        {
            for( const auto& [md5value, paths] : value )
            {
                const size_t filesCount = paths.size();
                if( filesCount > 1 )
                {
                    const CUL::String m_md5valueString = "MD5: " + md5value + ", files count: " + CUL::String( (int) filesCount );
                    if( ImGui::TreeNode( m_md5valueString.cStr() ) )
                    {
                        for( const auto& fsPath : paths )
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

    removeDeletedFilesFromDB();

    auto culFF = m_culInterface->getFS();

    startWorkers();

    for( const auto& path : m_searchPaths )
    {
        culFF->ListAllFiles( path, [this] ( const CUL::FS::Path& path ){
            m_foundFile = path.getPath();
            m_filesCount = m_filesCount + 1.f;
            addTask( [this, path] ( size_t workerId ){
                m_currentFiles[workerId] = path.getPath();
                addFile( path.getPath().string() );
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

    sqlite3_close( m_db );

    m_searchStarted = false;

    m_culInterface->getLogger()->log( "\nDONE.");
}

void App::searchBackground()
{
    auto culFF = m_culInterface->getFS();
    m_updateDeletedFiles.addTask( [this] (){
        m_culInterface->getLogger()->log( "removeDeletedFilesFromDB::START" );
        removeDeletedFilesFromDB();

        m_initialDbFilesUpdated = true;

        m_culInterface->getLogger()->log( "removeDeletedFilesFromDB::STOP" );
    } );

    m_updateDeletedFiles.setRemoveTasksWhenConsumed( true );
    m_updateDeletedFiles.setThreadName( "m_updateDeletedFiles" );
    m_updateDeletedFiles.SleepMS = 1000;
    m_updateDeletedFiles.run();
    m_runBackground = true;

    m_saveWorker.setRemoveTasksWhenConsumed( false );
    m_saveWorker.setThreadName( "m_saveWorker" );
    m_saveWorker.SleepMS = 16000;
    //m_saveWorker.run();
    m_saveWorker.addTask( [this] (){
        m_culInterface->getLogger()->log( "saveDuplicatesToFile::START" );
        saveDuplicatesToFile();
        m_culInterface->getLogger()->log( "saveDuplicatesToFile::STOP" );
    } );

    startWorkers();
    m_searchStarted = true;


    m_genericWorker.setThreadName( "m_genericWorker" );
    m_genericWorker.setRemoveTasksWhenConsumed( true );
    m_genericWorker.run();

    m_genericWorker.addTask( [this, culFF] (){
        for( const auto& m_searchPath : m_searchPaths )
        {
            culFF->ListAllFiles( m_searchPath, [this] ( const CUL::FS::Path& path ){

                if( !path.getIsDir() )
                {
                    m_foundFile = path.getPath();

                    m_filesCount = m_filesCount + 1.f;
                   
                    addTask( [this, path] ( size_t workerId ){
                        m_currentFiles[workerId] = path.getPath();
                        addFile( path.getPath().string() );
                        m_currentFiles[workerId] = "";

                        m_filesDone = m_filesDone + 1.f;
                    } );
                }
            } );
        }
    } );



    while( m_runBackground )
    {
        std::list<FileGroup>::iterator it;

        {
            std::lock_guard<std::mutex> lock( m_filesMtx );
            it = m_files.begin();
        }

        while( it != m_files.end() )
        {
            FileGroup& filePath = *it;

            for( const auto& file: filePath.files )
            {
                if( file.exists() )
                {
                    addTask( [this, file] ( size_t workerId ){
                        m_currentFiles[workerId] = file.getPath();
                        addFile( file.getPath().string() );
                        m_currentFiles[workerId] = "";

                        m_filesDone = m_filesDone + 1.f;
                    } );
                }
                else
                {
                    // here add to list for removal.
                }
            }
            std::lock_guard<std::mutex> lock( m_filesMtx );
            ++it;
        }
    }
}

void App::saveDuplicatesToFile()
{
    auto file = m_culInterface->getFF()->createRegularFileRawPtr( CUL::FS::Path( m_outputFile ) );

    CUL::String text = CUL::String( "Files: " );
    m_culInterface->getLogger()->log( text );
    file->addLine( text );

    std::map<FileSize, std::map<MD5Value, std::set<CUL::FS::Path>>> duplicatesCopy;

    {
        std::lock_guard<std::mutex> lock( m_duplicatesMtx );
        duplicatesCopy = m_duplicates;
    }

    for( const auto& sizesGroup : duplicatesCopy )
    {
        bool foundDups = false;
        for( const auto& md5Group : sizesGroup.second )
        {
            if( md5Group.second.size() > 1 )
            {
                foundDups = true;
                break;
            }
        }

        if( foundDups )
        {
            text = CUL::String( "Size: " ) + CUL::String( sizesGroup.first );

            file->addLine( text );

            for( const auto& md5Group : sizesGroup.second )
            {
                if( md5Group.second.size() > 1 )
                {
                    text = CUL::String( "MD5: " ) + CUL::String( md5Group.first );
                    file->addLine( text );
                    for( const auto& paths : md5Group.second )
                    {
                        file->addLine( paths );
                    }
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

void App::removeDeletedFilesFromDB()
{
    getList();

    const size_t deletionListSize = m_deletionList.size();

    for( size_t i = 0; i < deletionListSize; ++i )
    {
        CUL::FS::Path path = m_deletionList[i];
        if( !path.exists() )
        {
            removeFileFromDB( path );
        }
    }

    m_deletionList.clear();
}


void App::removeFileFromDB( const CUL::String& path )
{
    std::string sqlQuery = std::string( "DELETE FROM FILES WHERE PATH='" ) + path.string() + "';";

    char* zErrMsg = nullptr;
    auto callback = []( void*, int, char**, char** )
    {
        // DuplicateFinder::s_instance->callback( NotUsed, argc, argv, azColName );
        return 0;
    };

    int rc = sqlite3_exec( m_db, sqlQuery.c_str(), callback, 0, &zErrMsg );

    if( rc != SQLITE_OK )
    {
        CUL::Assert::simple( false, "DB ERROR!" );
    }
}


void App::addFile( const CUL::String& path )
{
    m_currentFileText = "Current file: " + path + ".";

    CUL::ITimer* m_frameTimer = CUL::TimerFactory::getChronoTimer( m_logger );
    m_frameTimer->start();

    std::unique_ptr<CUL::FS::IFile> file;
    file.reset( m_culInterface->getFF()->createRegularFileRawPtr( path ) );

    auto modTime = file->getLastModificationTime();
    auto modTimeFromDb = getModTimeFromDb( path );
    const auto modTimeFromFS = modTime.toString();

    CUL::String md5;
    CUL::String sizeBytes;

    if( path.contains( "science_in_this_shit" ) )
    {
        auto x = 0;
    }
    if( modTimeFromFS != modTimeFromDb )
    {
        md5 = file->getMD5();
        sizeBytes = file->getSizeBytes();
        addFileToDb( md5, path, CUL::String( sizeBytes ), modTime.toString() );
    }
    else
    {
        m_culInterface->getLogger()->log( CUL::String( "Path: " ) + path + " found on cache." );
        std::lock_guard<std::mutex> lockGuard( m_filesFromDbMtx );
        auto it = m_filesFromDb.find( path );
        md5 = it->second.md5;
        sizeBytes = it->second.size;
    }

    if( sizeBytes > m_minFileSize )
    {
        std::lock_guard<std::mutex> lockMap( m_filesMtx );

        auto it = std::find_if( m_files.begin(), m_files.end(), [path, sizeBytes, md5] ( const FileGroup& fg ){

            if( fg.fileSize == sizeBytes.toUInt() )
            {
                if( fg.md5 == md5 )
                {
                    return true;
                }
            }

            return false;
        } );

        if( it == m_files.end() )
        {
            FileGroup fg;
            fg.fileSize = sizeBytes.toUInt();
            fg.md5 = md5;
            fg.files.push_back( path );

            m_files.push_back(fg);

            m_files.sort( [] ( const FileGroup& fg1, const FileGroup& fg2 ){
                return fg1.fileSize < fg2.fileSize;
            } );
        }
        else
        {
            auto& filesOnSameSizeAndMD5 = it->files;

            auto fileIt = std::find_if( filesOnSameSizeAndMD5.begin(), filesOnSameSizeAndMD5.end(), [path] (const CUL::FS::Path& curr){
                return curr == path;
            } );

            if( fileIt == filesOnSameSizeAndMD5.end() )
            {

                filesOnSameSizeAndMD5.push_back( path );
                filesOnSameSizeAndMD5.sort();
            }

        }
    }

    if( sizeBytes > m_minFileSize )
    {
        addDuplicate( sizeBytes.toUInt(), md5, path );
    }
    


    m_frameTimer->stop();
    const auto& elapsed = m_frameTimer->getElapsed();
    const unsigned elapsedUi = elapsed.getMs();
    m_fileAddTasksDuration.push_back( elapsedUi );

    if( m_fileAddTasksDuration.size() > m_maxTasksDurationSamples )
    {
        m_fileAddTasksDuration.pop_front();
    }

    delete m_frameTimer;

    if( sizeBytes > m_minFileSize )
    {
        FileDb fileDb;
        fileDb.md5 = md5;
        fileDb.modTime = modTime.toString();
        fileDb.size = sizeBytes;
        fileDb.path = path;

        std::lock_guard<std::mutex> lockGuard( m_allFilesMtx );
        m_allFiles.push_back( fileDb );
        std::sort( m_allFiles.begin(), m_allFiles.end(),
                   [] ( const FileDb& fdb1, const FileDb& fdb2 )               {
            return fdb1.md5 < fdb2.md5;
        } );
    }
}

void App::addFileToList( const CUL::String path )
{
    //std::lock_guard<std::mutex> lockMap( m_allFilestMtx );
    //auto it = std::find( m_allFilesList.begin(), m_allFilesList.end(), path );
    //if( it == m_allFilesList.end() )
    //{
    //    m_allFilesList.push_back( path );
    //    std::sort( m_allFilesList.begin(), m_allFilesList.end() );
    //}

    //m_foundFile = path;
}

void App::addDuplicate( const FileSize fileSize, const MD5Value& md5, const CUL::FS::Path& inPath )
{
    std::lock_guard<std::mutex> lockIn( m_filesMtx );

    auto it = std::find_if( m_files.begin(), m_files.end(), [fileSize, md5] ( const FileGroup& fg ){

        if( fg.fileSize == fileSize )
        {
            if( fg.md5 == md5 )
            {
                return true;
            }
        }

        return false;
    } );

    if( it != m_files.end() )
    {
        auto fileIt = std::find_if( it->files.begin(), it->files.end(), [inPath] ( const CUL::FS::Path& path ){return inPath == path; } );

        if( fileIt != it->files.end() )
        {
            std::lock_guard<std::mutex> lockOut( m_duplicatesMtx );
            std::map<MD5Value, std::set<CUL::FS::Path>>& md5Map = m_duplicates[fileSize];

            std::set<CUL::FS::Path>& dupVec = md5Map[md5];
            dupVec.clear();

            for( const auto& path : it->files )
            {
                dupVec.insert( path );
            }
        }
    }
}

void App::addFileToDb( MD5Value md5, const CUL::String& filePath, const CUL::String& fileSize, const CUL::String& modTime )
{
    if( filePath.contains( "science_in_this_shit" ) )
    {
        auto x = 0;
    }

    CUL::String result;
    CUL::String filePathNormalized = filePath;
    filePathNormalized.replace( "./", "" );
    filePathNormalized.replace( "'", "''" );
    filePathNormalized.removeAll( '\0' );
    CUL::String sqlQuery = "INSERT INTO FILES (PATH, SIZE, MD5, LAST_MODIFICATION ) VALUES ('" + filePathNormalized + "', '" + fileSize +
                           "', '" + md5 + "', '" + modTime + "');";

    char* zErrMsg = nullptr;
    auto callback = []( void*, int, char**, char** )
    {
        // DuplicateFinder::s_instance->callback( NotUsed, argc, argv, azColName );
        return 0;
    };

    int rc = sqlite3_exec( m_db, sqlQuery.cStr(), callback, 0, &zErrMsg );

    if( rc != SQLITE_OK )
    {
        CUL::Assert::simple( false, "DB ERROR!" );
    }
}

CUL::String App::getModTimeFromDb( const CUL::String& filePath )
{
    getParametersFromDb( filePath );
    CUL::String result;
    std::lock_guard<std::mutex> lockGuard( m_filesFromDbMtx );
    auto it = m_filesFromDb.find( filePath );
    if( it != m_filesFromDb.end() )
    {
        result = it->second.modTime;
    }
    return result;
}


void App::getParametersFromDb( const CUL::String& filePath )
{
    if( filePath.contains( "science_in_this_shit" ) )
    {
        auto x = 0;
    }

    CUL::String filePathNormalized = filePath;
    filePathNormalized.replace( "./", "" );
    filePathNormalized.removeAll( '\0' );
    filePathNormalized.replace( "'", "''" );
    std::string sqlQuery =
        std::string( "SELECT PATH, SIZE, MD5, LAST_MODIFICATION FROM FILES WHERE PATH='" ) + filePathNormalized.string() + "';";
    char* zErrMsg = nullptr;



    auto callback = []( void*, int argc, char** argv, char** )
    {
        std::vector<CUL::String> values;
        for( int i = 0; i < argc; ++i )
        {
            CUL::String value = argv[i];
            values.push_back( value );
        }
        s_instance->addFileFromDb( values[0], values[1], values[2], values[3] );
        return 0;
    };

    int rc = sqlite3_exec( m_db, sqlQuery.c_str(), callback, 0, &zErrMsg );

    if( rc != SQLITE_OK )
    {
        CUL::Assert::simple( false, "DB ERROR!" );
    }
}


void App::addFileFromDb( const CUL::String& path, const CUL::String& size, const CUL::String& md, const CUL::String& modTime )
{
    std::lock_guard<std::mutex> lockGuard( m_filesFromDbMtx );

    if( path.contains("science_in_this_shit") )
    {
        auto x = 0;
    }

    FileDb fdb;
    fdb.size = size;
    fdb.md5 = md;
    fdb.modTime = modTime;
    m_filesFromDb[path] = fdb;
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
    std::string sqlQuery = std::string( "SELECT PATH, SIZE, MD5, LAST_MODIFICATION FROM FILES" );
    auto callback = []( void*, int /* argc */, char** argv, char** )
    {
        s_instance->addForCheckForDeletionList( argv[0] );
        return 0;
    };

    char* zErrMsg = nullptr;
    int rc = sqlite3_exec( m_db, sqlQuery.c_str(), callback, 0, &zErrMsg );

    if( rc != SQLITE_OK )
    {
        CUL::Assert::simple( false, "DB ERROR!" );
    }
}

void App::printCurrentMean()
{
    float sum = 0.f;
    std::lock_guard<std::mutex> lock( m_fileAddTasksDurationMtx );
    size_t vSize = m_fileAddTasksDuration.size();
    for( size_t i = 0; i < vSize; ++i )
    {
        sum += m_fileAddTasksDuration[i];
    }

    float mean = sum / vSize;

    m_culInterface->getLogger()->log( CUL::String( "Mean: " ) + CUL::String( mean ) );
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