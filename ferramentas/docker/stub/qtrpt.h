#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <QImage>
struct DataSetInfo { int recordCount = 0; QString reportPage; };
class QtRPT : public QObject {
    Q_OBJECT
public:
    explicit QtRPT(QObject *parent = nullptr);
    bool loadReport(QString fileName);
    void printExec(bool = false, bool = true, QString = QString());
    void printPDF(const QString &, bool = true);
signals:
    void setDSInfo(DataSetInfo &dsinfo);
    void setValue(const int recNo, const QString paramName, QVariant &paramValue, const int reportPage);
    void setValueImage(const int recNo, const QString paramName, QImage &paramValue, const int reportPage);
};
