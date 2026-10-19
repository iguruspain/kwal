#pragma once

#include <QString>

// Bundled templates (starship presets, ulauncher theme) are compiled into the
// executable and copied to the user's kwal config directory at startup.
namespace tools
{

// ~/.config/kwal/templates
QString templateDir();

// Copies the bundled templates into templateDir(), skipping files that
// already exist so user edits are never overwritten.
void installTemplates();

}
