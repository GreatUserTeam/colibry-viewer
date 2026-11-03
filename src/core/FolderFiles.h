#pragma once

#include <QString>
#include <QStringList>

namespace colibry::fv {

/// The files that live next to a given file, in natural order ("img2" before "img10").
/// Used for previous / next navigation. Folders are skipped, and so are hidden files
/// unless the starting file is hidden itself.
class FolderFiles
{
public:
    static QStringList siblings(const QString &path);
};

} // namespace colibry::fv
