#pragma once

#include "gameengine/IGameEngineApp.hpp"

#include "SDL2Wrapper/WindowData.hpp"
#include "SDL2Wrapper/IWindow.hpp"

#include "CUL/Threading/ThreadWrap.hpp"
#include "CUL/Math/Rotation.hpp"

NAMESPACE_BEGIN( LOGLW )
class Camera;
class Cube;
class Sprite;
class TransformComponent;
class Quad;
NAMESPACE_END( LOGLW )

class App final: public LOGLW::IGameEngineApp
{
public:
    App( bool fullscreen, unsigned width, unsigned height, int x, int y, const char* winName, const char* configPath );

    void addFileFromDb( const CUL::String& path, const CUL::String& size, const CUL::String& md, const CUL::String& modTime );
    void addForCheckForDeletionList( const CUL::String& path );

    int callback( void* NotUsed, int argc, char** argv, char** azColName );
    static App* s_instance;

    ~App();

protected:
private:
    void removeDeletedFilesFromDB();
    void getList();
    void removeFileFromDB( const CUL::String& path );

    struct FileDb
    {
        CUL::String size;
        CUL::String md5;
        CUL::String modTime;
        CUL::String path;
    };
    using FileSize = CUL::String;
    using MD5Value = CUL::String;
    using Value = std::map<MD5Value, std::vector<CUL::FS::Path>>;

    void onInit() override;
    void onWindowEvent( const SDL2W::WindowEvent::Type e ) override;
    void timerThread();
    void onKeyBoardEvent( const SDL2W::IKey& key ) override;
    void customFrame();
    void onMouseEvent( const SDL2W::MouseData& mouseData );
    void updateEuler( float yaw, float pitch );
    void guiIteration();
    void search( const std::string& path, const std::string& summaryFilePath );
    void addFile( const CUL::String& path );

    void addTask( std::function<void()> task );
    std::function<void()> getTask();
    void workerThreadMethod();
    unsigned getTasksLeft();
    void initDb();
    void addFileToDb( MD5Value md5, const CUL::String& filePath, const CUL::String& fileSize, const CUL::String& modTime );
    CUL::String getModTimeFromDb( const CUL::String& filePath );
    void getParametersFromDb( const CUL::String& filePath );
    void printCurrentMean();

    glm::vec3 moveOnSphere( float yaw, float pitch, float row, float rad );

    std::atomic<bool> m_runTimer = true;
    CUL::ThreadWrapper m_thread;

    LOGLW::Camera* m_camera = nullptr;

    SDL2W::MouseData m_mouseData;

    unsigned m_indices[4];
    unsigned m_bufferId = 0;
    float m_time = 0.0f;
    float m_angle = 0.f;
    float m_deg = 0.f;

    CUL::MATH::Rotation m_lookAngles;

    int m_mouseLastX = 0;
    int m_mouseLastY = 0;

    float m_yawLast = 0.f;
    float m_pitchLast = 0.f;

    glm::vec3 m_front = { 0.f, 0.f, 0.f };
    glm::vec3 m_right = { 0.f, 0.f, 0.f };
    float m_velocity = 0.2f;
    bool m_firstMouse = true;


    CUL::CULInterface* m_culInterface = nullptr;
    size_t m_maxThreadCount = 10;

    std::mutex m_duplicatesMtx;
    std::map<FileSize, std::map<MD5Value, std::set<CUL::FS::Path>>> m_duplicates;

    std::mutex m_filesPathsMapMtx;
    std::map<FileSize, Value> m_filesPathsMap;

    std::mutex m_workersMtx;
    std::map<std::thread::id, std::thread> m_workers;

    std::mutex m_tasksMtx;
    std::vector<std::function<void()>> m_tasks;

    std::mutex m_filesFromDbMtx;
    std::map<CUL::String, FileDb> m_filesFromDb;

    std::mutex m_sqliteMtx;

    float m_filesCount = 0.f;
    float m_filesLeft = 0.f;

    int m_last = 101;

    struct sqlite3* m_db = nullptr;
    CUL::String m_empty;

    std::mutex m_fileAddTasksDurationMtx;
    std::vector<unsigned> m_fileAddTasksDuration;

    std::vector<CUL::String> m_deletionList;

    std::mutex m_allFilesMtx;
    std::vector<FileDb> m_allFiles;

    std::thread m_searchThread;
};