#pragma once

#include <QString>

// templates are compiled into the executable and copied to the config dir at startup.
namespace tools
{

QString templateDir();

// never overwrites existing files (user edits).
void installTemplates();

}
