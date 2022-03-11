#include "DuplicateFinder.hpp"

int main( int argc, char** argv )
{
    std::vector<std::string> arguments;
    for( int i = 0; i < argc; ++i )
    {
        arguments.push_back( std::string( argv[(size_t)i] ) );
    }

    std::string searchPath = ".";
    if( argc > 1 )
    {
        searchPath = arguments[1];
    }

    DuplicateFinder df;
    df.search( searchPath, "Result.txt" );

    return 0;
}