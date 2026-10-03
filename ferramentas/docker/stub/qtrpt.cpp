#include "qtrpt.h"
QtRPT::QtRPT(QObject *parent) : QObject(parent) {}
bool QtRPT::loadReport(QString) { return true; }
void QtRPT::printExec(bool, bool, QString) {}
void QtRPT::printPDF(const QString &, bool) {}
