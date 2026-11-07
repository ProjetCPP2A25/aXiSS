#ifndef XLSXWRITER_H
#define XLSXWRITER_H

// Écriture minimale d'un classeur .xlsx (sans dépendance externe) :
// un .xlsx est une archive ZIP de fichiers XML ; on écrit ici une archive "stored"
// (sans compression) avec les quelques fichiers requis par Excel.

#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QFile>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Xlsx {

inline quint32 crc32(const QByteArray &data)
{
    static quint32 table[256];
    static bool ready = false;
    if (!ready) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = true;
    }
    quint32 c = 0xFFFFFFFFu;
    for (char ch : data)
        c = table[(c ^ static_cast<quint8>(ch)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

inline QString columnName(int index)   // 0 -> A, 25 -> Z, 26 -> AA
{
    QString s;
    for (int n = index; n >= 0; n = n / 26 - 1)
        s.prepend(QChar('A' + n % 26));
    return s;
}

inline QByteArray cell(int col, int row, const QString &text, bool bold)
{
    return QStringLiteral("<c r=\"%1%2\" t=\"inlineStr\"%3><is><t xml:space=\"preserve\">%4</t></is></c>")
        .arg(columnName(col)).arg(row)
        .arg(bold ? QStringLiteral(" s=\"1\"") : QString())
        .arg(text.toHtmlEscaped())
        .toUtf8();
}

inline QByteArray sheetXml(const QStringList &header, const QVector<QStringList> &rows,
                           const QList<int> &widths)
{
    QByteArray x = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                   "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                   "<sheetViews><sheetView workbookViewId=\"0\"><pane ySplit=\"1\" topLeftCell=\"A2\" "
                   "activePane=\"bottomLeft\" state=\"frozen\"/></sheetView></sheetViews><cols>";
    for (int c = 0; c < widths.size(); ++c)
        x += QStringLiteral("<col min=\"%1\" max=\"%1\" width=\"%2\" customWidth=\"1\"/>")
                 .arg(c + 1).arg(widths[c]).toUtf8();
    x += "</cols><sheetData>";

    x += "<row r=\"1\">";
    for (int c = 0; c < header.size(); ++c)
        x += cell(c, 1, header[c], true);
    x += "</row>";

    for (int r = 0; r < rows.size(); ++r) {
        x += QStringLiteral("<row r=\"%1\">").arg(r + 2).toUtf8();
        for (int c = 0; c < rows[r].size(); ++c)
            x += cell(c, r + 2, rows[r][c], false);
        x += "</row>";
    }
    x += "</sheetData></worksheet>";
    return x;
}

inline bool write(const QString &path, const QString &sheetName, const QStringList &header,
                  const QVector<QStringList> &rows, const QList<int> &widths, QString *error)
{
    const QByteArray xmlHead = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";

    QList<QPair<QString, QByteArray>> files;
    files << qMakePair(QStringLiteral("[Content_Types].xml"),
        xmlHead + "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "</Types>");
    files << qMakePair(QStringLiteral("_rels/.rels"),
        xmlHead + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>");
    files << qMakePair(QStringLiteral("xl/workbook.xml"),
        xmlHead + "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets><sheet name=\"" + sheetName.toHtmlEscaped().toUtf8() + "\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>");
    files << qMakePair(QStringLiteral("xl/_rels/workbook.xml.rels"),
        xmlHead + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>");
    files << qMakePair(QStringLiteral("xl/styles.xml"),
        xmlHead + "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<fonts count=\"2\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font>"
        "<font><b/><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>"
        "<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill><fill><patternFill patternType=\"gray125\"/></fill></fills>"
        "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"2\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/></cellXfs>"
        "</styleSheet>");
    files << qMakePair(QStringLiteral("xl/worksheets/sheet1.xml"), sheetXml(header, rows, widths));

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = out.errorString();
        return false;
    }

    const QDateTime now = QDateTime::currentDateTime();
    const quint16 dosTime = quint16((now.time().hour() << 11) | (now.time().minute() << 5) | (now.time().second() / 2));
    const quint16 dosDate = quint16(((now.date().year() - 1980) << 9) | (now.date().month() << 5) | now.date().day());

    QDataStream s(&out);
    s.setByteOrder(QDataStream::LittleEndian);

    struct Entry { QByteArray name; quint32 crc; quint32 size; quint32 offset; };
    QVector<Entry> entries;

    for (const auto &f : files) {
        Entry e;
        e.name = f.first.toUtf8();
        e.crc = crc32(f.second);
        e.size = quint32(f.second.size());
        e.offset = quint32(out.pos());
        entries.push_back(e);

        s << quint32(0x04034b50) << quint16(20) << quint16(0x0800) << quint16(0)
          << dosTime << dosDate << e.crc << e.size << e.size
          << quint16(e.name.size()) << quint16(0);
        s.writeRawData(e.name.constData(), e.name.size());
        s.writeRawData(f.second.constData(), f.second.size());
    }

    const quint32 centralStart = quint32(out.pos());
    for (const Entry &e : entries) {
        s << quint32(0x02014b50) << quint16(20) << quint16(20) << quint16(0x0800) << quint16(0)
          << dosTime << dosDate << e.crc << e.size << e.size
          << quint16(e.name.size()) << quint16(0) << quint16(0)
          << quint16(0) << quint16(0) << quint32(0) << e.offset;
        s.writeRawData(e.name.constData(), e.name.size());
    }
    const quint32 centralSize = quint32(out.pos()) - centralStart;

    s << quint32(0x06054b50) << quint16(0) << quint16(0)
      << quint16(entries.size()) << quint16(entries.size())
      << centralSize << centralStart << quint16(0);

    out.close();
    if (s.status() != QDataStream::Ok) {
        if (error) *error = QStringLiteral("Erreur d'écriture du fichier.");
        return false;
    }
    return true;
}

} // namespace Xlsx

#endif // XLSXWRITER_H
