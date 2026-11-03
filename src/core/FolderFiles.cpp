#include "FolderFiles.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace colibry::fv {

namespace {

/// "img2" < "img10", and letter case is ignored. Does not depend on the locale or on ICU.
bool naturalLess(const QString &a, const QString &b)
{
    const QString x = a.toCaseFolded();
    const QString y = b.toCaseFolded();
    qsizetype i = 0;
    qsizetype j = 0;

    while (i < x.size() && j < y.size()) {
        if (x.at(i).isDigit() && y.at(j).isDigit()) {
            qsizetype ie = i;
            qsizetype je = j;
            while (ie < x.size() && x.at(ie).isDigit())
                ++ie;
            while (je < y.size() && y.at(je).isDigit())
                ++je;

            // Compare as numbers: ignore leading zeros, then shorter means smaller.
            QStringView nx = QStringView(x).mid(i, ie - i);
            QStringView ny = QStringView(y).mid(j, je - j);
            while (nx.size() > 1 && nx.at(0) == QLatin1Char('0'))
                nx = nx.mid(1);
            while (ny.size() > 1 && ny.at(0) == QLatin1Char('0'))
                ny = ny.mid(1);
            if (nx.size() != ny.size())
                return nx.size() < ny.size();
            if (const int c = nx.compare(ny))
                return c < 0;
            i = ie;
            j = je;
        } else {
            if (x.at(i) != y.at(j))
                return x.at(i) < y.at(j);
            ++i;
            ++j;
        }
    }
    if (x.size() - i != y.size() - j)
        return (x.size() - i) < (y.size() - j);
    return a < b; // equal ignoring case: keep the order stable and total
}

} // namespace

QStringList FolderFiles::siblings(const QString &path)
{
    const QFileInfo start(path);
    QDir dir(start.absolutePath());

    QDir::Filters filters = QDir::Files | QDir::NoDotAndDotDot;
    if (start.isHidden())
        filters |= QDir::Hidden;

    QStringList names = dir.entryList(filters);

    std::sort(names.begin(), names.end(), naturalLess);

    QStringList result;
    result.reserve(names.size());
    for (const QString &name : std::as_const(names))
        result << dir.filePath(name);
    return result;
}

} // namespace colibry::fv
