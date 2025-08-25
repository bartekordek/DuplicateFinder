#include "DuplicateFinderBase.hpp"
#include <CUL/STL_IMPORTS/STD_algorithm.hpp>

DuplicateFinderBase::DuplicateFinderBase()
{

}


void DuplicateFinderBase::addPath( const CUL::String& inPath )
{
    std::lock_guard<std::mutex> locker( m_searchPathsMtx );
    m_searchPaths.push_back(inPath);
    std::sort( m_searchPaths.begin(), m_searchPaths.end() );
}

void DuplicateFinderBase::removePath(const CUL::String& inPath)
{
    std::lock_guard<std::mutex> locker( m_searchPathsMtx );

    const auto it = std::find( m_searchPaths.begin(), m_searchPaths.end(), inPath );
    if (it != m_searchPaths.end())
    {
        m_searchPaths.erase(it);
    }
}

void DuplicateFinderBase::removeLast()
{
    std::lock_guard<std::mutex> locker( m_searchPathsMtx );
    m_searchPaths.pop_back();
}

const PathList DuplicateFinderBase::getPaths() const
{
    std::lock_guard<std::mutex> locker( m_searchPathsMtx );
    return m_searchPaths;
}

DuplicateFinderBase::~DuplicateFinderBase()
{
}