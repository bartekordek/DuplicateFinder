#pragma once

#include "IMPORT_wxwidgets.hpp"
#include "CUL/UselessMacros.hpp"

class wxButton;

class MyApp: public wxApp
{
public:
    MyApp();
    bool OnInit() override;

private:

public:
    CUL_NONCOPYABLE( MyApp );
};
