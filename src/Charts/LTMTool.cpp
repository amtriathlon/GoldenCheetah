/*
 * Copyright (c) 2010 Mark Liversedge (liversedge@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "LTMTool.h"
#include "MainWindow.h"
#include "Context.h"
#include "Athlete.h"
#include "Settings.h"
#include "Units.h"
#include "Tab.h"
#include "RideNavigator.h"
#include "HelpWhatsThis.h"
#include "Utils.h"

#include <QApplication>
#include <QtGui>

// charts.xml support
#include "LTMChartParser.h"

// seasons.xml support
#include "Season.h"
#include "SeasonParser.h"
#include <QXmlInputSource>
#include <QXmlSimpleReader>

// metadata.xml support
#include "RideMetadata.h"
#include "SpecialFields.h"

// PDModel estimate support
#include "PDModel.h"

// Filter / formula
#include "DataFilter.h"

LTMTool::LTMTool(Context *context, LTMSettings *settings) : QWidget(context->mainWindow), settings(settings), context(context), active(false), _amFiltered(false)
{
    setStyleSheet("QFrame { FrameStyle = QFrame::NoFrame };"
                  "QWidget { background = Qt::white; border:0 px; margin: 2px; };");

    // get application settings
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);
    mainLayout->setSpacing(0);
    setContentsMargins(0,0,0,0);

    //----------------------------------------------------------------------------------------------------------
    // Basic Settings (First TAB)
    //----------------------------------------------------------------------------------------------------------
    basicsettings = new QWidget(this);
    HelpWhatsThis *basicHelp = new HelpWhatsThis(basicsettings);
    basicsettings->setWhatsThis(basicHelp->getWhatsThisText(HelpWhatsThis::ChartTrends_MetricTrends_Config_Basic));

    QFormLayout *basicsettingsLayout = new QFormLayout(basicsettings);

    searchBox = new SearchFilterBox(this, context);
    HelpWhatsThis *searchHelp = new HelpWhatsThis(searchBox);
    searchBox->setWhatsThis(searchHelp->getWhatsThisText(HelpWhatsThis::SearchFilterBox));
    connect(searchBox, SIGNAL(searchClear()), this, SLOT(clearFilter()));
    connect(searchBox, SIGNAL(searchResults(QStringList)), this, SLOT(setFilter(QStringList)));

    basicsettingsLayout->addRow(new QLabel(tr("Filter")), searchBox);
    basicsettingsLayout->addRow(new QLabel(tr(""))); // spacing

    dateSetting = new DateSettingsEdit(this);
    HelpWhatsThis *dateSettingHelp = new HelpWhatsThis(dateSetting);
    dateSetting->setWhatsThis(dateSettingHelp->getWhatsThisText(HelpWhatsThis::ChartTrends_DateRange));
    basicsettingsLayout->addRow(new QLabel(tr("Date range")), dateSetting);
    basicsettingsLayout->addRow(new QLabel(tr(""))); // spacing

    groupBy = new QComboBox;
    groupBy->addItem(tr("Days"), LTM_DAY);
    groupBy->addItem(tr("Weeks"), LTM_WEEK);
    groupBy->addItem(tr("Months"), LTM_MONTH);
    groupBy->addItem(tr("Years"), LTM_YEAR);
    groupBy->addItem(tr("Time Of Day"), LTM_TOD);
    groupBy->addItem(tr("All"), LTM_ALL);
    groupBy->setCurrentIndex(0);
    basicsettingsLayout->addRow(new QLabel(tr("Group by")), groupBy);
    basicsettingsLayout->addRow(new QLabel(tr(""))); // spacing

    showData = new QCheckBox(tr("Data Table"));
    showData->setChecked(false);
    basicsettingsLayout->addRow(new QLabel(""), showData);

    showStack = new QCheckBox(tr("Show Stack"));
    showStack->setChecked(false);
    basicsettingsLayout->addRow(new QLabel(""), showStack);

    shadeZones = new QCheckBox(tr("Shade Zones"));
    basicsettingsLayout->addRow(new QLabel(""), shadeZones);

    showLegend = new QCheckBox(tr("Show Legend"));
    basicsettingsLayout->addRow(new QLabel(""), showLegend);

    showEvents = new QCheckBox(tr("Show Events"));
    basicsettingsLayout->addRow(new QLabel(""), showEvents);

    stackSlider = new QSlider(Qt::Horizontal,this);
    stackSlider->setMinimum(0);
    stackSlider->setMaximum(7);
    stackSlider->setTickInterval(1);
    stackSlider->setValue(3);
    stackSlider->setFixedWidth(100);
    basicsettingsLayout->addRow(new QLabel(tr("Stack Zoom")), stackSlider);
    // use separate line to distinguish from the operational buttons for the Table View

    usePreset = new QCheckBox(tr("Use sidebar chart settings"));
    usePreset->setChecked(false);
    basicsettingsLayout->addRow(new QLabel(""), new QLabel());
    basicsettingsLayout->addRow(new QLabel(""), usePreset);

    //----------------------------------------------------------------------------------------------------------
    // Preset List (2nd TAB)
    //----------------------------------------------------------------------------------------------------------

    presets = new QWidget(this);

    presets->setContentsMargins(20,20,20,20);
    HelpWhatsThis *presetHelp = new HelpWhatsThis(presets);
    presets->setWhatsThis(presetHelp->getWhatsThisText(HelpWhatsThis::ChartTrends_MetricTrends_Config_Preset));
    QVBoxLayout *presetLayout = new QVBoxLayout(presets);
    presetLayout->setContentsMargins(0,0,0,0);
    presetLayout->setSpacing(5);

    charts = new QTreeWidget;
#ifdef Q_OS_MAC
    charts->setAttribute(Qt::WA_MacShowFocusRect, 0);
#endif
    charts->headerItem()->setText(0, tr("Charts"));
    charts->setColumnCount(1);
    charts->setSelectionMode(QAbstractItemView::SingleSelection);
    charts->setEditTriggers(QAbstractItemView::SelectedClicked); // allow edit
    charts->setIndentation(0);

    presetLayout->addWidget(charts, 0,0);

    applyButton = new QPushButton(tr("Apply")); // connected in LTMWindow.cpp (weird!?)
    newButton = new QPushButton(tr("Add Current"));
    connect(newButton, SIGNAL(clicked()), this, SLOT(addCurrent()));

    QHBoxLayout *presetButtons = new QHBoxLayout;
    presetButtons->addWidget(applyButton);
    presetButtons->addStretch();
    presetButtons->addWidget(newButton);

    presetLayout->addLayout(presetButtons);



    //----------------------------------------------------------------------------------------------------------
    // initialise the metrics catalogue and user selector (for Custom Curves - 4th TAB)
    //----------------------------------------------------------------------------------------------------------

    const RideMetricFactory &factory = RideMetricFactory::instance();
    for (int i = 0; i < factory.metricCount(); ++i) {

        // metrics catalogue and settings
        MetricDetail adds;
        QColor cHSV;

        adds.symbol = factory.metricName(i);
        adds.metric = factory.rideMetric(factory.metricName(i));
        qsrand(QTime::currentTime().msec());
        cHSV.setHsv((i%6)*(255/(factory.metricCount()/5)), 255, 255);
        adds.penColor = cHSV.convertTo(QColor::Rgb);
        adds.curveStyle = curveStyle(factory.metricType(i));
        adds.symbolStyle = symbolStyle(factory.metricType(i));
        adds.smooth = false;
        adds.trendtype = 0;
        adds.topN = 1; // show top 1 by default always

        adds.name   = Utils::unprotect(adds.metric->name());

        // set default for the user overiddable fields
        adds.uname  = adds.name;
        adds.units = adds.metric->units(context->athlete->useMetricUnits);
        adds.uunits = adds.units;

        // default units to metric name if it is blank
        if (adds.uunits == "") adds.uunits = adds.name;
        metrics.append(adds);
    }

    // metadata metrics
    SpecialFields sp;
    foreach (FieldDefinition field, context->athlete->rideMetadata()->getFields()) {
        if (!sp.isMetric(field.name) && (field.type == 3 || field.type == 4)) {
            MetricDetail metametric;
            metametric.type = METRIC_META;
            QString underscored = field.name;
            metametric.symbol = underscored.replace(" ", "_");
            metametric.metric = NULL; // not a factory metric
            metametric.penColor = QColor(Qt::blue);
            metametric.curveStyle = QwtPlotCurve::Lines;
            metametric.symbolStyle = QwtSymbol::NoSymbol;
            metametric.smooth = false;
            metametric.trendtype = 0;
            metametric.topN = 1;
            metametric.uname = metametric.name = sp.displayName(field.name);
            metametric.units = "";
            metametric.uunits = "";
            metrics.append(metametric);
        }
    }

    // sort the list
    qSort(metrics);

    //----------------------------------------------------------------------------------------------------------
    // Custom Curves (4th TAB)
    //----------------------------------------------------------------------------------------------------------

    custom = new QWidget(this);
    custom->setContentsMargins(20,20,20,20);
    HelpWhatsThis *curvesHelp = new HelpWhatsThis(custom);
    custom->setWhatsThis(curvesHelp->getWhatsThisText(HelpWhatsThis::ChartTrends_MetricTrends_Config_Curves));
    QVBoxLayout *customLayout = new QVBoxLayout(custom);
    customLayout->setContentsMargins(0,0,0,0);
    customLayout->setSpacing(5);

    // custom table
    customTable = new QTableWidget(this);
#ifdef Q_OS_MAX
    customTable->setAttribute(Qt::WA_MacShowFocusRect, 0);
#endif
    customTable->setColumnCount(2);
    customTable->horizontalHeader()->setStretchLastSection(true);
    customTable->setSortingEnabled(false);
    customTable->verticalHeader()->hide();
    customTable->setShowGrid(false);
    customTable->setSelectionMode(QAbstractItemView::SingleSelection);
    customTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    customLayout->addWidget(customTable);
    connect(customTable, SIGNAL(cellDoubleClicked(int, int)), this, SLOT(doubleClicked(int, int)));

    // custom buttons
    editCustomButton = new QPushButton(tr("Edit"));
    connect(editCustomButton, SIGNAL(clicked()), this, SLOT(editMetric()));

    addCustomButton = new QPushButton("+");
    connect(addCustomButton, SIGNAL(clicked()), this, SLOT(addMetric()));

    deleteCustomButton = new QPushButton("-");
    connect(deleteCustomButton, SIGNAL(clicked()), this, SLOT(deleteMetric()));

#ifndef Q_OS_MAC
    upCustomButton = new QToolButton(this);
    downCustomButton = new QToolButton(this);
    upCustomButton->setArrowType(Qt::UpArrow);
    downCustomButton->setArrowType(Qt::DownArrow);
    upCustomButton->setFixedSize(20,20);
    downCustomButton->setFixedSize(20,20);
    addCustomButton->setFixedSize(20,20);
    deleteCustomButton->setFixedSize(20,20);
#else
    upCustomButton = new QPushButton(tr("Up"));
    downCustomButton = new QPushButton(tr("Down"));
#endif
    connect(upCustomButton, SIGNAL(clicked()), this, SLOT(moveMetricUp()));
    connect(downCustomButton, SIGNAL(clicked()), this, SLOT(moveMetricDown()));


    QHBoxLayout *customButtons = new QHBoxLayout;
    customButtons->setSpacing(2);
    customButtons->addWidget(upCustomButton);
    customButtons->addWidget(downCustomButton);
    customButtons->addStretch();
    customButtons->addWidget(editCustomButton);
    customButtons->addStretch();
    customButtons->addWidget(addCustomButton);
    customButtons->addWidget(deleteCustomButton);
    customLayout->addLayout(customButtons);

    //----------------------------------------------------------------------------------------------------------
    // setup the Tabs
    //----------------------------------------------------------------------------------------------------------

    tabs = new QTabWidget(this);
    mainLayout->addWidget(tabs);
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    tabs->addTab(basicsettings, tr("Basic"));
    tabs->addTab(presets, tr("Preset"));
    tabs->addTab(custom, tr("Curves"));

    // switched between one or other
    connect(dateSetting, SIGNAL(useStandardRange()), this, SIGNAL(useStandardRange()));
    connect(dateSetting, SIGNAL(useCustomRange(DateRange)), this, SIGNAL(useCustomRange(DateRange)));
    connect(dateSetting, SIGNAL(useThruToday()), this, SIGNAL(useThruToday()));

    // watch for changes to the preset charts
    connect(context, SIGNAL(presetsChanged()), this, SLOT(presetsChanged()));
    connect(usePreset, SIGNAL(stateChanged(int)), this, SLOT(usePresetChanged()));

    // set the show/hide for preset selection
    usePresetChanged();
    
    // but setup for the first time
    presetsChanged();
}

void
LTMTool::hideBasic()
{
    // first make sure use sidebar is false
    usePreset->setChecked(false);
    if (tabs->count() == 3) {

        tabs->removeTab(0);
        basicsettings->hide(); // it doesn't get deleted

        // resize etc
        tabs->updateGeometry();
        presets->updateGeometry();
        custom->updateGeometry();

        // choose curves tab
        tabs->setCurrentIndex(1);
    }
}

void
LTMTool::usePresetChanged()
{
    customTable->setEnabled(!usePreset->isChecked());
    editCustomButton->setEnabled(!usePreset->isChecked());
    addCustomButton->setEnabled(!usePreset->isChecked());
    deleteCustomButton->setEnabled(!usePreset->isChecked());
    upCustomButton->setEnabled(!usePreset->isChecked());
    downCustomButton->setEnabled(!usePreset->isChecked());

    // yuck .. this doesn't work nicely !
    //basic->setHidden(usePreset->isChecked());
    //custom->setHidden(usePreset->isChecked());
    // so instead we disable
    charts->setEnabled(!usePreset->isChecked());
    newButton->setEnabled(!usePreset->isChecked());
    applyButton->setEnabled(!usePreset->isChecked());

}

void
LTMTool::presetsChanged()
{
    // rebuild the preset chart list as the presets have changed
    charts->clear();
    foreach(LTMSettings chart, context->athlete->presets) {
        QTreeWidgetItem *add;
        add = new QTreeWidgetItem(charts->invisibleRootItem());
        add->setFlags(add->flags() & ~Qt::ItemIsEditable);
        add->setText(0, chart.name);
    }

    // select the first one, if there are any
    if (context->athlete->presets.count())
        charts->setCurrentItem(charts->invisibleRootItem()->child(0));
}


void
LTMTool::refreshCustomTable(int indexSelectedItem)
{
    // clear then repopulate custom table settings to reflect
    // the current LTMSettings.
    customTable->clear();

    // get headers back
    QStringList header;
    header << tr("Type") << tr("Details"); 
    customTable->setHorizontalHeaderLabels(header);

    QTableWidgetItem *selected = new QTableWidgetItem();
    // now lets add a row for each metric
    customTable->setRowCount(settings->metrics.count());
    int i=0;
    foreach (MetricDetail metricDetail, settings->metrics) {

        QTableWidgetItem *t = new QTableWidgetItem();
        if (metricDetail.type < 5)
            t->setText(tr("Metric")); // only metrics .. for now ..
        else if (metricDetail.type == 5)
            t->setText(tr("Peak"));
        else if (metricDetail.type == 6)
            t->setText(tr("Estimate"));
        else if (metricDetail.type == 7)
            t->setText(tr("Stress"));
        else if (metricDetail.type == 8)
            t->setText(tr("Formula"));

        t->setFlags(t->flags() & (~Qt::ItemIsEditable));
        customTable->setItem(i,0,t);

        t = new QTableWidgetItem();
        if (metricDetail.type == 8) {
            t->setText(metricDetail.formula);
        } else if (metricDetail.type != 5 && metricDetail.type != 6)
            t->setText(metricDetail.name);
        else {
            // text description for peak
            t->setText(metricDetail.uname);
        }

        t->setFlags(t->flags() & (~Qt::ItemIsEditable));
        customTable->setItem(i,1,t);

        // keep the selected item from previous step (relevant for moving up/down)
        if (indexSelectedItem == i) {
            selected = t;
        }

        i++;
    }

    if (selected) {
      customTable->setCurrentItem(selected);
    }

}

void
LTMTool::editMetric()
{
    QList<QTableWidgetItem*> items = customTable->selectedItems();
    if (items.count() < 1) return;

    int index = customTable->row(items.first());

    MetricDetail edit = settings->metrics[index];
    EditMetricDetailDialog dialog(context, this, &edit);

    if (dialog.exec()) {

        // apply!
        settings->metrics[index] = edit;

        // update
        refreshCustomTable();
        curvesChanged();
    }
}

void
LTMTool::doubleClicked( int row, int column )
{
    (void) column; // ignore, calm down

    MetricDetail edit = settings->metrics[row];
    EditMetricDetailDialog dialog(context, this, &edit);

    if (dialog.exec()) {

        // apply!
        settings->metrics[row] = edit;

        // update
        refreshCustomTable();
        curvesChanged();
    }
}

void
LTMTool::deleteMetric()
{
    QList<QTableWidgetItem*> items = customTable->selectedItems();
    if (items.count() < 1) return;
    
    int index = customTable->row(items.first());
    settings->metrics.removeAt(index);
    refreshCustomTable();
    curvesChanged();
}

void
LTMTool::addMetric()
{
    MetricDetail add;
    EditMetricDetailDialog dialog(context, this, &add);

    if (dialog.exec()) {
        // apply
        settings->metrics.append(add);

        // refresh
        refreshCustomTable();
        curvesChanged();
    }
}

void
LTMTool::moveMetricUp()
{
    QList<QTableWidgetItem*> items = customTable->selectedItems();
    if (items.count() < 1) return;

    int index = customTable->row(items.first());

    if (index > 0) {
        settings->metrics.swap(index, index-1);
         // refresh
        refreshCustomTable(index-1);
        curvesChanged();
    }
}

void
LTMTool::moveMetricDown()
{
    QList<QTableWidgetItem*> items = customTable->selectedItems();
    if (items.count() < 1) return;

    int index = customTable->row(items.first());

    if (index+1 <  settings->metrics.size()) {
        settings->metrics.swap(index, index+1);
         // refresh
        refreshCustomTable(index+1);
        curvesChanged();
    }
}



void
LTMTool::applySettings()
{
    foreach (MetricDetail metricDetail, settings->metrics) {
        // get index for the symbol
        for (int i=0; i<metrics.count(); i++) {

            if (metrics[i].symbol == metricDetail.symbol) {

                // rather than copy each member one by one
                // we save the ridemetric pointer and metric type
                // copy across them all then re-instate the saved point
                RideMetric *saved = (RideMetric*)metrics[i].metric;
                int type = metrics[i].type;

                metrics[i] = metricDetail;
                metrics[i].metric = saved;
                metrics[i].type = type;

                // units may need to be adjusted if
                // usemetricUnits changed since charts.xml was
                // written
                if (saved && saved->conversion() != 1.0 &&
                    metrics[i].uunits.contains(saved->units(!context->athlete->useMetricUnits)))
                    metrics[i].uunits.replace(saved->units(!context->athlete->useMetricUnits), saved->units(context->athlete->useMetricUnits));


                break;
            }
        }
    }

    refreshCustomTable();

    curvesChanged();
}

void
LTMTool::addCurrent()
{
    // give the chart a name
    if (settings->name == "") settings->name = QString(tr("Chart %1")).arg(context->athlete->presets.count()+1);

    // add the current chart to the presets with a name using the chart title
    context->athlete->presets.append(*settings);

    // tree will now be refreshed
    context->notifyPresetsChanged();
}

// set the estimateSelection based upon what is available
void 
EditMetricDetailDialog::modelChanged()
{
    int currentIndex=modelSelect->currentIndex();
    int ce = estimateSelect->currentIndex();

    // pooey this smells ! -- enable of disable each option 
    //                        based upon the current model
    qobject_cast<QStandardItemModel *>(estimateSelect->model())->item(0)->setEnabled(models[currentIndex]->hasWPrime());
    qobject_cast<QStandardItemModel *>(estimateSelect->model())->item(1)->setEnabled(models[currentIndex]->hasCP());
    qobject_cast<QStandardItemModel *>(estimateSelect->model())->item(2)->setEnabled(models[currentIndex]->hasFTP());
    qobject_cast<QStandardItemModel *>(estimateSelect->model())->item(3)->setEnabled(models[currentIndex]->hasPMax());
    qobject_cast<QStandardItemModel *>(estimateSelect->model())->item(4)->setEnabled(true);
    qobject_cast<QStandardItemModel *>(estimateSelect->model())->item(5)->setEnabled(true);
    qobject_cast<QStandardItemModel *>(estimateSelect->model())->item(6)->setEnabled(true);

    // switch to other estimate if wanted estimate is not selected
    if (ce < 0 || !qobject_cast<QStandardItemModel *>(estimateSelect->model())->item(ce)->isEnabled())
        estimateSelect->setCurrentIndex(0);

    estimateName();
}

void
EditMetricDetailDialog::estimateChanged()
{
    estimateName();
}

void
EditMetricDetailDialog::estimateName()
{
    if (chooseEstimate->isChecked() == false) return; // only if we have estimate selected

    // do we need to see the best duration ?
    if (estimateSelect->currentIndex() == 4) {
        estimateDuration->show();
        estimateDurationUnits->show();
    } else {
        estimateDuration->hide();
        estimateDurationUnits->hide();
    }

    // set the estimate name from model and estimate type
    QString name;

    // first do the type if estimate
    switch(estimateSelect->currentIndex()) {
        case 0 : name = "W'"; break;
        case 1 : name = "CP"; break;
        case 2 : name = "FTP"; break;
        case 3 : name = "p-Max"; break;
        case 4 : 
            {
                name = QString(tr("Estimate %1 %2 Power")).arg(estimateDuration->value())
                                                  .arg(estimateDurationUnits->currentText());
            }
            break;
        case 5 : name = tr("Endurance Index"); break;
        case 6 : name = tr("Vo2Max Estimate"); break;
    }

    // now the model
    name += " (" + models[modelSelect->currentIndex()]->code() + ")";
    userName->setText(name);
    metricDetail->symbol = name.replace(" ", "_");
}

/*----------------------------------------------------------------------
 * EDIT METRIC DETAIL DIALOG
 *--------------------------------------------------------------------*/

static bool insensitiveLessThan(const QString &a, const QString &b)
{
    return a.toLower() < b.toLower();
}

EditMetricDetailDialog::EditMetricDetailDialog(Context *context, LTMTool *ltmTool, MetricDetail *metricDetail) :
    QDialog(context->mainWindow, Qt::Dialog), context(context), ltmTool(ltmTool), metricDetail(metricDetail)
{
    setWindowTitle(tr("Curve Settings"));

    HelpWhatsThis *help = new HelpWhatsThis(this);
    this->setWhatsThis(help->getWhatsThisText(HelpWhatsThis::ChartTrends_MetricTrends_Curves_Settings));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // choose the type
    chooseMetric = new QRadioButton(tr("Metric"), this);
    chooseBest = new QRadioButton(tr("Best"), this);
    chooseEstimate = new QRadioButton(tr("Estimate"), this);
    chooseStress = new QRadioButton(tr("Stress"), this);
    chooseFormula = new QRadioButton(tr("Formula"), this);

    // put them into a button group because we
    // also have radio buttons for watts per kilo / absolute
    group = new QButtonGroup(this);
    group->addButton(chooseMetric);
    group->addButton(chooseBest);
    group->addButton(chooseEstimate);
    group->addButton(chooseStress);
    group->addButton(chooseFormula);

    // uncheck them all
    chooseMetric->setChecked(false);
    chooseBest->setChecked(false);
    chooseEstimate->setChecked(false);
    chooseStress->setChecked(false);
    chooseFormula->setChecked(false);

    // which one ?
    switch (metricDetail->type) {
    default:
        chooseMetric->setChecked(true);
        break;
    case 5:
        chooseBest->setChecked(true);
        break;
    case 6:
        chooseEstimate->setChecked(true);
        break;
    case 7:
        chooseStress->setChecked(true);
        break;
    case 8:
        chooseFormula->setChecked(true);
        break;
    }

    QVBoxLayout *radioButtons = new QVBoxLayout;
    radioButtons->addStretch();
    radioButtons->addWidget(chooseMetric);
    radioButtons->addWidget(chooseBest);
    radioButtons->addWidget(chooseEstimate);
    radioButtons->addWidget(chooseStress);
    radioButtons->addWidget(chooseFormula);
    radioButtons->addStretch();

    // bests selection
    duration = new QDoubleSpinBox(this);
    duration->setDecimals(0);
    duration->setMinimum(0);
    duration->setMaximum(999);
    duration->setSingleStep(1.0);
    duration->setValue(metricDetail->duration); // default to 60 minutes

    durationUnits = new QComboBox(this);
    durationUnits->addItem(tr("seconds"));
    durationUnits->addItem(tr("minutes"));
    durationUnits->addItem(tr("hours"));
    switch(metricDetail->duration_units) {
        case 1 : durationUnits->setCurrentIndex(0); break;
        case 60 : durationUnits->setCurrentIndex(1); break;
        default :
        case 3600 : durationUnits->setCurrentIndex(2); break;
    }

    dataSeries = new QComboBox(this);

    // add all the different series supported
    seriesList << RideFile::watts
               << RideFile::wattsKg
               << RideFile::xPower
               << RideFile::aPower
               << RideFile::NP
               << RideFile::hr
               << RideFile::kph
               << RideFile::cad
               << RideFile::nm
               << RideFile::vam;

    foreach (RideFile::SeriesType x, seriesList) {
            dataSeries->addItem(RideFile::seriesName(x), static_cast<int>(x));
    }

    int index = seriesList.indexOf(metricDetail->series);
    if (index < 0) index = 0;
    dataSeries->setCurrentIndex(index);

    bestWidget = new QWidget(this);
    QVBoxLayout *alignLayout = new QVBoxLayout(bestWidget);
    QGridLayout *bestLayout = new QGridLayout();
    alignLayout->addLayout(bestLayout);

    bestLayout->addWidget(duration, 0,0);
    bestLayout->addWidget(durationUnits, 0,1);
    bestLayout->addWidget(new QLabel(tr("Peak"), this), 1,0);
    bestLayout->addWidget(dataSeries, 1,1);

    // estimate selection
    estimateWidget = new QWidget(this);
    QVBoxLayout *estimateLayout = new QVBoxLayout(estimateWidget);

    modelSelect = new QComboBox(this);
    estimateSelect = new QComboBox(this);

    // working with estimates, local utility functions
    models << new CP2Model(context);
    models << new CP3Model(context);
    models << new MultiModel(context);
    models << new ExtendedModel(context);
    models << new WSModel(context);
    foreach(PDModel *model, models) {
        modelSelect->addItem(model->name(), model->code());
    }

    estimateSelect->addItem("W'");
    estimateSelect->addItem("CP");
    estimateSelect->addItem("FTP");
    estimateSelect->addItem("p-Max");
    estimateSelect->addItem("Best Power");
    estimateSelect->addItem("Endurance Index");
    estimateSelect->addItem("Vo2Max Estimate");

    int n=0;
    modelSelect->setCurrentIndex(0); // default to 2parm model
    foreach(PDModel *model, models) {
        if (model->code() == metricDetail->model) modelSelect->setCurrentIndex(n);
        else n++;
    }
    estimateSelect->setCurrentIndex(metricDetail->estimate);

    estimateDuration = new QDoubleSpinBox(this);
    estimateDuration->setDecimals(0);
    estimateDuration->setMinimum(0);
    estimateDuration->setMaximum(999);
    estimateDuration->setSingleStep(1.0);
    estimateDuration->setValue(metricDetail->estimateDuration); // default to 60 minutes

    estimateDurationUnits = new QComboBox(this);
    estimateDurationUnits->addItem(tr("seconds"));
    estimateDurationUnits->addItem(tr("minutes"));
    estimateDurationUnits->addItem(tr("hours"));
    switch(metricDetail->estimateDuration_units) {
        case 1 : estimateDurationUnits->setCurrentIndex(0); break;
        case 60 : estimateDurationUnits->setCurrentIndex(1); break;
        default :
        case 3600 : estimateDurationUnits->setCurrentIndex(2); break;
    }
    QHBoxLayout *estbestLayout = new QHBoxLayout();
    estbestLayout->addWidget(estimateDuration);
    estbestLayout->addWidget(estimateDurationUnits);

    // estimate as absolute or watts per kilo ?
    abs = new QRadioButton(tr("Absolute"), this);
    wpk = new QRadioButton(tr("Per Kilogram"), this);
    wpk->setChecked(metricDetail->wpk);
    abs->setChecked(!metricDetail->wpk);

    QHBoxLayout *estwpk = new QHBoxLayout;
    estwpk->addStretch();
    estwpk->addWidget(abs);
    estwpk->addWidget(wpk);
    estwpk->addStretch();

    estimateLayout->addStretch();
    estimateLayout->addWidget(modelSelect);
    estimateLayout->addWidget(estimateSelect);
    estimateLayout->addLayout(estbestLayout);
    estimateLayout->addLayout(estwpk);
    estimateLayout->addStretch();

    // estimate selection
    formulaWidget = new QWidget(this);
    //formulaWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *formulaLayout = new QVBoxLayout(formulaWidget);

    // courier font
    formulaEdit = new DataFilterEdit(this, context);
    QFont courier("Courier", QFont().pointSize());
    QFontMetrics fm(courier);

    formulaEdit->setFont(courier);
    formulaEdit->setTabStopWidth(4 * fm.width(' ')); // 4 char tabstop
    //formulaEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    formulaType = new QComboBox(this);
    formulaType->addItem(tr("Total"), static_cast<int>(RideMetric::Total));
    formulaType->addItem(tr("Running Total"), static_cast<int>(RideMetric::RunningTotal));
    formulaType->addItem(tr("Average"), static_cast<int>(RideMetric::Average));
    formulaType->addItem(tr("Peak"), static_cast<int>(RideMetric::Peak));
    formulaType->addItem(tr("Low"), static_cast<int>(RideMetric::Low));
    formulaLayout->addWidget(formulaEdit);
    QHBoxLayout *ftype = new QHBoxLayout;
    ftype->addWidget(new QLabel(tr("Aggregate:")));
    ftype->addWidget(formulaType);
    ftype->addStretch();
    formulaLayout->addLayout(ftype);

    // set to the value...
    if (metricDetail->formula == "") {
        // lets put a template in there
        metricDetail->formula = tr("# type in a formula to use\n" 
                                   "# for e.g. TSS / Duration\n"
                                   "# as you type the available metrics\n"
                                   "# will be offered by autocomplete\n");
    }
    formulaEdit->setText(metricDetail->formula);
    formulaType->setCurrentIndex(formulaType->findData(metricDetail->formulaType));

    // get suitably formated list
    QList<QString> list;
    QString last;
    SpecialFields sp;

    // get sorted list
    QStringList names = context->tab->rideNavigator()->logicalHeadings;

    // start with just a list of functions
    list = DataFilter::builtins();

    // ridefile data series symbols
    list += RideFile::symbols();

    // add special functions (older code needs fixing !)
    list << "config(cranklength)";
    list << "config(cp)";
    list << "config(ftp)";
    list << "config(w')";
    list << "config(pmax)";
    list << "config(cv)";
    list << "config(scv)";
    list << "config(height)";
    list << "config(weight)";
    list << "config(lthr)";
    list << "config(maxhr)";
    list << "config(rhr)";
    list << "config(units)";
    list << "const(e)";
    list << "const(pi)";
    list << "daterange(start)";
    list << "daterange(stop)";
    list << "ctl";
    list << "tsb";
    list << "atl";
    list << "sb(TSS)";
    list << "lts(TSS)";
    list << "sts(TSS)";
    list << "rr(TSS)";
    list << "tiz(power, 1)";
    list << "tiz(hr, 1)";
    list << "best(power, 3600)";
    list << "best(hr, 3600)";
    list << "best(cadence, 3600)";
    list << "best(speed, 3600)";
    list << "best(torque, 3600)";
    list << "best(np, 3600)";
    list << "best(xpower, 3600)";
    list << "best(vam, 3600)";
    list << "best(wpk, 3600)";

    qSort(names.begin(), names.end(), insensitiveLessThan);

    foreach(QString name, names) {

        // handle dups
        if (last == name) continue;
        last = name;

        // Handle bikescore tm
        if (name.startsWith("BikeScore")) name = QString("BikeScore");

        //  Always use the "internalNames" in Filter expressions
        name = sp.internalName(name);

        // we do very little to the name, just space to _ and lower case it for now...
        name.replace(' ', '_');
        list << name;
    }

    // set new list
    // create an empty completer, configchanged will fix it
    DataFilterCompleter *completer = new DataFilterCompleter(list, this);
    formulaEdit->setCompleter(completer);

    // stress selection
    stressTypeSelect = new QComboBox(this);
    stressTypeSelect->addItem(tr("Short Term Stress (STS/ATL)"), STRESS_STS);
    stressTypeSelect->addItem(tr("Long Term Stress  (LTS/CTL)"), STRESS_LTS);
    stressTypeSelect->addItem(tr("Stress Balance    (SB/TSB)"),  STRESS_SB);
    stressTypeSelect->addItem(tr("Stress Ramp Rate  (RR)"),      STRESS_RR);
    stressTypeSelect->addItem(tr("Planned Short Term Stress (STS/ATL)"), STRESS_PLANNED_STS);
    stressTypeSelect->addItem(tr("Planned Long Term Stress  (LTS/CTL)"), STRESS_PLANNED_LTS);
    stressTypeSelect->addItem(tr("Planned Stress Balance    (SB/TSB)"),  STRESS_PLANNED_SB);
    stressTypeSelect->addItem(tr("Planned Stress Ramp Rate  (RR)"),      STRESS_PLANNED_RR);
    stressTypeSelect->addItem(tr("Expected Short Term Stress (STS/ATL)"), STRESS_EXPECTED_STS);
    stressTypeSelect->addItem(tr("Expected Long Term Stress  (LTS/CTL)"), STRESS_EXPECTED_LTS);
    stressTypeSelect->addItem(tr("Expected Stress Balance    (SB/TSB)"),  STRESS_EXPECTED_SB);
    stressTypeSelect->addItem(tr("Expected Stress Ramp Rate  (RR)"),      STRESS_EXPECTED_RR);
    stressTypeSelect->setCurrentIndex(metricDetail->stressType);

    stressWidget = new QWidget(this);
    stressWidget->setContentsMargins(0,0,0,0);
    QHBoxLayout *stressLayout = new QHBoxLayout(stressWidget);
    stressLayout->setContentsMargins(0,0,0,0);
    stressLayout->setSpacing(5);
    stressLayout->addWidget(new QLabel(tr("Stress Type"), this));
    stressLayout->addWidget(stressTypeSelect);

    metricWidget = new QWidget(this);
    metricWidget->setContentsMargins(0,0,0,0);
    QVBoxLayout *metricLayout = new QVBoxLayout(metricWidget);

    // metric selection tree
    metricTree = new QTreeWidget;
    metricLayout->addWidget(metricTree);

    // and add the stress selector to this widget
    // too as we reuse it for stress selection
    metricLayout->addWidget(stressWidget);

#ifdef Q_OS_MAC
    metricTree->setAttribute(Qt::WA_MacShowFocusRect, 0);
#endif
    metricTree->setColumnCount(1);
    metricTree->setSelectionMode(QAbstractItemView::SingleSelection);
    metricTree->header()->hide();
    metricTree->setIndentation(5);

    foreach(MetricDetail metric, ltmTool->metrics) {
        QTreeWidgetItem *add;
        add = new QTreeWidgetItem(metricTree->invisibleRootItem(), METRIC_TYPE);
        add->setText(0, metric.name);
        if (metric.metric != NULL) add->setToolTip(0, metric.metric->description());
        else if (metric.type == METRIC_META) add->setToolTip(0, tr("Metadata Field"));
        else if (metric.type == METRIC_PM) add->setToolTip(0, tr("PMC metric"));
    }
    metricTree->expandItem(metricTree->invisibleRootItem());

    index = indexMetric(metricDetail);
    if (index > 0) {

        // which item to select?
        QTreeWidgetItem *item = metricTree->invisibleRootItem()->child(index);

        // select the current
        metricTree->clearSelection();
        metricTree->setCurrentItem(item, QItemSelectionModel::Select);
    }

    // contains all the different ways of defining
    // a curve, one foreach type. currently just
    // metric and bests, but will add formula and
    // measure at some point
    typeStack = new QStackedWidget(this);
    typeStack->addWidget(metricWidget);
    typeStack->addWidget(bestWidget);
    typeStack->addWidget(estimateWidget);
    typeStack->addWidget(formulaWidget);
    typeStack->setCurrentIndex(chooseMetric->isChecked() ? 0 : (chooseBest->isChecked() ? 1 : 2));

    // Grid
    QGridLayout *grid = new QGridLayout;

    QLabel *filter = new QLabel(tr("Filter"));
    dataFilter = new SearchFilterBox(this, context);
    dataFilter->setFilter(metricDetail->datafilter);

    QLabel *name = new QLabel(tr("Name"));
    QLabel *units = new QLabel(tr("Axis Label / Units"));
    userName = new QLineEdit(this);
    userName->setText(metricDetail->uname);
    userUnits = new QLineEdit(this);
    userUnits->setText(metricDetail->uunits);

    QLabel *style = new QLabel(tr("Style"));
    curveStyle = new QComboBox(this);
    curveStyle->addItem(tr("Bar"), QwtPlotCurve::Steps);
    curveStyle->addItem(tr("Line"), QwtPlotCurve::Lines);
    curveStyle->addItem(tr("Sticks"), QwtPlotCurve::Sticks);
    curveStyle->addItem(tr("Dots"), QwtPlotCurve::Dots);
    curveStyle->setCurrentIndex(curveStyle->findData(metricDetail->curveStyle));

    QLabel *stackLabel = new QLabel(tr("Stack"));
    stack = new QCheckBox("", this);
    stack->setChecked(metricDetail->stack);


    QLabel *symbol = new QLabel(tr("Symbol"));
    curveSymbol = new QComboBox(this);
    curveSymbol->addItem(tr("None"), QwtSymbol::NoSymbol);
    curveSymbol->addItem(tr("Circle"), QwtSymbol::Ellipse);
    curveSymbol->addItem(tr("Square"), QwtSymbol::Rect);
    curveSymbol->addItem(tr("Diamond"), QwtSymbol::Diamond);
    curveSymbol->addItem(tr("Triangle"), QwtSymbol::Triangle);
    curveSymbol->addItem(tr("Cross"), QwtSymbol::XCross);
    curveSymbol->addItem(tr("Hexagon"), QwtSymbol::Hexagon);
    curveSymbol->addItem(tr("Star"), QwtSymbol::Star1);
    curveSymbol->setCurrentIndex(curveSymbol->findData(metricDetail->symbolStyle));

    QLabel *color = new QLabel(tr("Color"));
    curveColor = new QPushButton(this);

    QLabel *fill = new QLabel(tr("Fill curve"));
    fillCurve = new QCheckBox("", this);
    fillCurve->setChecked(metricDetail->fillCurve);
 
    labels = new QCheckBox(tr("Data labels"), this);
    labels->setChecked(metricDetail->labels);
 
    // color background...
    penColor = metricDetail->penColor;
    setButtonIcon(penColor);

    QLabel *topN = new QLabel(tr("Highlight Highest"));
    showBest = new QDoubleSpinBox(this);
    showBest->setDecimals(0);
    showBest->setMinimum(0);
    showBest->setMaximum(999);
    showBest->setSingleStep(1.0);
    showBest->setValue(metricDetail->topN);

    QLabel *bottomN = new QLabel(tr("Highlight Lowest"));
    showLowest = new QDoubleSpinBox(this);
    showLowest->setDecimals(0);
    showLowest->setMinimum(0);
    showLowest->setMaximum(999);
    showLowest->setSingleStep(1.0);
    showLowest->setValue(metricDetail->lowestN);

    QLabel *outN = new QLabel(tr("Highlight Outliers"));
    showOut = new QDoubleSpinBox(this);
    showOut->setDecimals(0);
    showOut->setMinimum(0);
    showOut->setMaximum(999);
    showOut->setSingleStep(1.0);
    showOut->setValue(metricDetail->topOut);

    QLabel *baseline = new QLabel(tr("Baseline"));
    baseLine = new QDoubleSpinBox(this);
    baseLine->setDecimals(0);
    baseLine->setMinimum(-999999);
    baseLine->setMaximum(999999);
    baseLine->setSingleStep(1.0);
    baseLine->setValue(metricDetail->baseline);

    curveSmooth = new QCheckBox(tr("Smooth Curve"), this);
    curveSmooth->setChecked(metricDetail->smooth);

    trendType = new QComboBox(this);
    trendType->addItem(tr("No trend Line"));
    trendType->addItem(tr("Linear Trend"));
    trendType->addItem(tr("Quadratic Trend"));
    trendType->addItem(tr("Moving Average"));
    trendType->setCurrentIndex(metricDetail->trendtype);

    // add to grid
    grid->addWidget(filter, 0,0);
    grid->addWidget(dataFilter, 0,1,1,3);
    grid->addLayout(radioButtons, 1, 0, 1, 1, Qt::AlignTop|Qt::AlignLeft);
    grid->addWidget(typeStack, 1, 1, 1, 3);
    QWidget *spacer1 = new QWidget(this);
    spacer1->setFixedHeight(10);
    grid->addWidget(spacer1, 2,0);
    grid->addWidget(name, 3,0);
    grid->addWidget(userName, 3, 1, 1, 3);
    grid->addWidget(units, 4,0);
    grid->addWidget(userUnits, 4,1);
    grid->addWidget(style, 5,0);
    grid->addWidget(curveStyle, 5,1);
    grid->addWidget(symbol, 6,0);
    grid->addWidget(curveSymbol, 6,1);
    QWidget *spacer2 = new QWidget(this);
    spacer2->setFixedHeight(10);
    grid->addWidget(spacer2, 7,0);
    grid->addWidget(stackLabel, 8, 0);
    grid->addWidget(stack, 8, 1);
    grid->addWidget(color, 9,0);
    grid->addWidget(curveColor, 9,1);
    grid->addWidget(fill, 10,0);
    grid->addWidget(fillCurve, 10,1);
    grid->addWidget(topN, 4,2);
    grid->addWidget(showBest, 4,3);
    grid->addWidget(bottomN, 5,2);
    grid->addWidget(showLowest, 5,3);
    grid->addWidget(outN, 6,2);
    grid->addWidget(showOut, 6,3);
    grid->addWidget(baseline, 7, 2);
    grid->addWidget(baseLine, 7,3);
    grid->addWidget(trendType, 8,2);
    grid->addWidget(curveSmooth, 9,2);
    grid->addWidget(labels, 10,2);

    mainLayout->addLayout(grid);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    applyButton = new QPushButton(tr("&OK"), this);
    cancelButton = new QPushButton(tr("&Cancel"), this);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(applyButton);
    mainLayout->addLayout(buttonLayout);

    // clean up the widgets
    typeChanged();
    modelChanged();

    // connect up slots
    connect(metricTree, SIGNAL(itemSelectionChanged()), this, SLOT(metricSelected()));
    connect(applyButton, SIGNAL(clicked()), this, SLOT(applyClicked()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    connect(curveColor, SIGNAL(clicked()), this, SLOT(colorClicked()));
    connect(chooseMetric, SIGNAL(toggled(bool)), this, SLOT(typeChanged()));
    connect(chooseBest, SIGNAL(toggled(bool)), this, SLOT(typeChanged()));
    connect(chooseEstimate, SIGNAL(toggled(bool)), this, SLOT(typeChanged()));
    connect(chooseStress, SIGNAL(toggled(bool)), this, SLOT(typeChanged()));
    connect(chooseFormula, SIGNAL(toggled(bool)), this, SLOT(typeChanged()));
    connect(modelSelect, SIGNAL(currentIndexChanged(int)), this, SLOT(modelChanged()));
    connect(estimateSelect, SIGNAL(currentIndexChanged(int)), this, SLOT(estimateChanged()));
    connect(estimateDuration, SIGNAL(valueChanged(double)), this, SLOT(estimateName()));
    connect(estimateDurationUnits, SIGNAL(currentIndexChanged(int)), this, SLOT(estimateName()));

    // when stuff changes rebuild name
    connect(chooseBest, SIGNAL(toggled(bool)), this, SLOT(bestName()));
    connect(chooseStress, SIGNAL(toggled(bool)), this, SLOT(metricSelected()));
    connect(chooseEstimate, SIGNAL(toggled(bool)), this, SLOT(estimateName()));
    connect(stressTypeSelect, SIGNAL(currentIndexChanged(int)), this, SLOT(metricSelected()));
    connect(chooseMetric, SIGNAL(toggled(bool)), this, SLOT(metricSelected()));
    connect(duration, SIGNAL(valueChanged(double)), this, SLOT(bestName()));
    connect(durationUnits, SIGNAL(currentIndexChanged(int)), this, SLOT(bestName()));
    connect(dataSeries, SIGNAL(currentIndexChanged(int)), this, SLOT(bestName()));
}

int
EditMetricDetailDialog::indexMetric(MetricDetail *metricDetail)
{
    for (int i=0; i < ltmTool->metrics.count(); i++) {
        if (ltmTool->metrics.at(i).symbol == metricDetail->symbol) return i;
    }
    return -1;
}

void
EditMetricDetailDialog::typeChanged()
{
    // switch stack and hide other
    if (chooseMetric->isChecked()) {
        bestWidget->hide();
        metricWidget->show();
        estimateWidget->hide();
        stressWidget->hide();
        formulaWidget->hide();
        typeStack->setCurrentIndex(0);
    }

    if (chooseBest->isChecked()) {
        bestWidget->show();
        metricWidget->hide();
        estimateWidget->hide();
        stressWidget->hide();
        formulaWidget->hide();
        typeStack->setCurrentIndex(1);
    }

    if (chooseEstimate->isChecked()) {
        bestWidget->hide();
        metricWidget->hide();
        estimateWidget->show();
        stressWidget->hide();
        formulaWidget->hide();
        typeStack->setCurrentIndex(2);
    }

    if (chooseStress->isChecked()) {
        bestWidget->hide();
        metricWidget->show();
        estimateWidget->hide();
        stressWidget->show();
        formulaWidget->hide();
        typeStack->setCurrentIndex(0);
    }

    if (chooseFormula->isChecked()) {
        formulaWidget->show();
        bestWidget->hide();
        metricWidget->hide();
        estimateWidget->hide();
        stressWidget->hide();
        typeStack->setCurrentIndex(3);
    }
    adjustSize();
}

void
EditMetricDetailDialog::stressDetails()
{
    // used when adding the generated curve to the curves
    // map in LTMPlot, we need to be able to differentiate
    // between adding the metric to a chart and adding
    // a stress series to a chart

    // only for bests!
    if (chooseStress->isChecked() == false) return;

    // re-use bestSymbol
    metricDetail->bestSymbol = metricDetail->symbol;

    // append type
    switch(stressTypeSelect->currentIndex()) {
    case STRESS_LTS:
    case STRESS_PLANNED_LTS:
    case STRESS_EXPECTED_LTS:
        metricDetail->bestSymbol += "_lts";
        metricDetail->penColor = QColor(Qt::blue);
        metricDetail->curveStyle = QwtPlotCurve::Lines;
        metricDetail->symbolStyle = QwtSymbol::NoSymbol;
        metricDetail->smooth = false;
        metricDetail->trendtype = 0;
        metricDetail->topN = 1;
        metricDetail->units = "Stress";
        metricDetail->uunits = "Stress";
        break;
    case STRESS_STS:
    case STRESS_PLANNED_STS:
    case STRESS_EXPECTED_STS:
        metricDetail->bestSymbol += "_sts";
        metricDetail->penColor = QColor(Qt::magenta);
        metricDetail->curveStyle = QwtPlotCurve::Lines;
        metricDetail->symbolStyle = QwtSymbol::NoSymbol;
        metricDetail->smooth = false;
        metricDetail->trendtype = 0;
        metricDetail->topN = 1;
        metricDetail->units = "Stress";
        metricDetail->uunits = "Stress";
        break;
    case STRESS_SB:
    case STRESS_PLANNED_SB:
    case STRESS_EXPECTED_SB:
        metricDetail->bestSymbol += "_sb";
        metricDetail->penColor = QColor(Qt::yellow);
        metricDetail->curveStyle = QwtPlotCurve::Lines;
        metricDetail->symbolStyle = QwtSymbol::NoSymbol;
        metricDetail->smooth = false;
        metricDetail->trendtype = 0;
        metricDetail->topN = 1;
        metricDetail->lowestN = 1;
        metricDetail->units = "Stress";
        metricDetail->uunits = "Stress";
        metricDetail->fillCurve = true;
        break;
    case STRESS_RR:
    case STRESS_PLANNED_RR:
    case STRESS_EXPECTED_RR:
        metricDetail->bestSymbol += "_rr";
        metricDetail->penColor = QColor(Qt::darkGreen);
        metricDetail->curveStyle = QwtPlotCurve::Lines;
        metricDetail->symbolStyle = QwtSymbol::NoSymbol;
        metricDetail->smooth = false;
        metricDetail->trendtype = 0;
        metricDetail->topN = 1;
        metricDetail->units = "Ramp";
        metricDetail->uunits = tr("Ramp");
        break;
    }

    // preppend planned/expected
    switch(stressTypeSelect->currentIndex()) {
    case STRESS_PLANNED_LTS:
    case STRESS_PLANNED_STS:
    case STRESS_PLANNED_SB:
    case STRESS_PLANNED_RR:
        metricDetail->bestSymbol = "planned_" + metricDetail->bestSymbol;
        metricDetail->curveStyle = QwtPlotCurve::Sticks;
        break;
    case STRESS_EXPECTED_LTS:
    case STRESS_EXPECTED_STS:
    case STRESS_EXPECTED_SB:
    case STRESS_EXPECTED_RR:
        metricDetail->bestSymbol = "expected_" + metricDetail->bestSymbol;
        metricDetail->curveStyle = QwtPlotCurve::Dots;
        break;
    }
}

void
EditMetricDetailDialog::bestName()
{
    // only for bests!
    if (chooseBest->isChecked() == false) return;

    // when widget destroyed we get negative indexes so ignore
    if (durationUnits->currentIndex() < 0 || dataSeries->currentIndex() < 0) return;

    // set uname from current parms
    QString desc = QString(tr("Peak %1")).arg(duration->value());
    switch (durationUnits->currentIndex()) {
    case 0 : desc += tr(" second "); break;
    case 1 : desc += tr(" minute "); break;
    default:
    case 2 : desc += tr(" hour "); break;
    }
    desc += RideFile::seriesName(seriesList.at(dataSeries->currentIndex()));
    userName->setText(desc);
    metricDetail->bestSymbol = desc.replace(" ", "_");
}

void
EditMetricDetailDialog::metricSelected()
{
    // only in metric mode
    if (!chooseMetric->isChecked() && !chooseStress->isChecked()) return;

    // user selected a different metric
    // so update accordingly
    int index = metricTree->invisibleRootItem()->indexOfChild(metricTree->currentItem());

    // out of bounds !
    if (index < 0 || index >= ltmTool->metrics.count()) return;

    (*metricDetail) = ltmTool->metrics[index]; // overwrite!

    // make the stress name & details
    if (chooseStress->isChecked()) stressDetails();

    userName->setText(metricDetail->uname);
    userUnits->setText(metricDetail->uunits);
    curveSmooth->setChecked(metricDetail->smooth);
    fillCurve->setChecked(metricDetail->fillCurve);
    labels->setChecked(metricDetail->labels);
    stack->setChecked(metricDetail->stack);
    showBest->setValue(metricDetail->topN);
    showOut->setValue(metricDetail->topOut);
    baseLine->setValue(metricDetail->baseline);
    penColor = metricDetail->penColor;
    trendType->setCurrentIndex(metricDetail->trendtype);
    setButtonIcon(penColor);

    // curve style
    switch (metricDetail->curveStyle) {
      
    case QwtPlotCurve::Steps:
        curveStyle->setCurrentIndex(0);
        break;
    case QwtPlotCurve::Lines:
        curveStyle->setCurrentIndex(1);
        break;
    case QwtPlotCurve::Sticks:
        curveStyle->setCurrentIndex(2);
        break;
    case QwtPlotCurve::Dots:
    default:
        curveStyle->setCurrentIndex(3);
        break;

    }

    // curveSymbol
    switch (metricDetail->symbolStyle) {
      
    case QwtSymbol::NoSymbol:
        curveSymbol->setCurrentIndex(0);
        break;
    case QwtSymbol::Ellipse:
        curveSymbol->setCurrentIndex(1);
        break;
    case QwtSymbol::Rect:
        curveSymbol->setCurrentIndex(2);
        break;
    case QwtSymbol::Diamond:
        curveSymbol->setCurrentIndex(3);
        break;
    case QwtSymbol::Triangle:
        curveSymbol->setCurrentIndex(4);
        break;
    case QwtSymbol::XCross:
        curveSymbol->setCurrentIndex(5);
        break;
    case QwtSymbol::Hexagon:
        curveSymbol->setCurrentIndex(6);
        break;
    case QwtSymbol::Star1:
    default:
        curveSymbol->setCurrentIndex(7);
        break;

    }
}

// uh. i hate enums when you need to modify from ints
// this is fugly and prone to error. Tied directly to the
// combo box above. all better solutions gratefully received
// but wanna get this code running for now
static QwtPlotCurve::CurveStyle styleMap[] = { QwtPlotCurve::Steps, QwtPlotCurve::Lines,
                                               QwtPlotCurve::Sticks, QwtPlotCurve::Dots };
static QwtSymbol::Style symbolMap[] = { QwtSymbol::NoSymbol, QwtSymbol::Ellipse, QwtSymbol::Rect,
                                        QwtSymbol::Diamond, QwtSymbol::Triangle, QwtSymbol::XCross,
                                        QwtSymbol::Hexagon, QwtSymbol::Star1 };
void
EditMetricDetailDialog::applyClicked()
{
    if (chooseMetric->isChecked()) { // is a metric, but what type?
        int index = indexMetric(metricDetail);
        if (index >= 0) metricDetail->type = ltmTool->metrics.at(index).type;
        else metricDetail->type = 1;
    }

    if (chooseBest->isChecked()) metricDetail->type = 5; // is a best
    else if (chooseEstimate->isChecked()) metricDetail->type = 6; // estimate
    else if (chooseStress->isChecked()) metricDetail->type = 7; // stress
    else if (chooseFormula->isChecked()) metricDetail->type = 8; // stress

    metricDetail->estimateDuration = estimateDuration->value();
    switch (estimateDurationUnits->currentIndex()) {
        case 0 : metricDetail->estimateDuration_units = 1; break;
        case 1 : metricDetail->estimateDuration_units = 60; break;
        case 2 :
        default: metricDetail->estimateDuration_units = 3600; break;
    }

    metricDetail->duration = duration->value();
    switch (durationUnits->currentIndex()) {
        case 0 : metricDetail->duration_units = 1; break;
        case 1 : metricDetail->duration_units = 60; break;
        case 2 :
        default: metricDetail->duration_units = 3600; break;
    }
    metricDetail->datafilter = dataFilter->filter();
    metricDetail->wpk = wpk->isChecked();
    metricDetail->series = seriesList.at(dataSeries->currentIndex());
    metricDetail->model = models[modelSelect->currentIndex()]->code();
    metricDetail->estimate = estimateSelect->currentIndex(); // 0 - 5
    metricDetail->smooth = curveSmooth->isChecked();
    metricDetail->topN = showBest->value();
    metricDetail->lowestN = showLowest->value();
    metricDetail->topOut = showOut->value();
    metricDetail->baseline = baseLine->value();
    metricDetail->curveStyle = styleMap[curveStyle->currentIndex()];
    metricDetail->symbolStyle = symbolMap[curveSymbol->currentIndex()];
    metricDetail->penColor = penColor;
    metricDetail->fillCurve = fillCurve->isChecked();
    metricDetail->labels = labels->isChecked();
    metricDetail->uname = userName->text();
    metricDetail->uunits = userUnits->text();
    metricDetail->stack = stack->isChecked();
    metricDetail->trendtype = trendType->currentIndex();
    metricDetail->stressType = stressTypeSelect->currentIndex();
    metricDetail->formula = formulaEdit->toPlainText();
    metricDetail->formulaType = static_cast<RideMetric::MetricType>(formulaType->itemData(formulaType->currentIndex()).toInt());
    accept();
}

QwtPlotCurve::CurveStyle
LTMTool::curveStyle(RideMetric::MetricType type)
{
    switch (type) {

    case RideMetric::Average : return QwtPlotCurve::Lines;
    case RideMetric::Total : return QwtPlotCurve::Steps;
    case RideMetric::Peak : return QwtPlotCurve::Lines;
    default : return QwtPlotCurve::Lines;

    }
}

QwtSymbol::Style
LTMTool::symbolStyle(RideMetric::MetricType type)
{
    switch (type) {

    case RideMetric::Average : return QwtSymbol::Ellipse;
    case RideMetric::Total : return QwtSymbol::Ellipse;
    case RideMetric::Peak : return QwtSymbol::Rect;
    default : return QwtSymbol::XCross;
    }
}
void
EditMetricDetailDialog::cancelClicked()
{
    reject();
}

void
EditMetricDetailDialog::colorClicked()
{
    QColorDialog picker(context->mainWindow);
    picker.setCurrentColor(penColor);

    // don't use native dialog, since there is a nasty bug causing focus loss
    // see https://bugreports.qt-project.org/browse/QTBUG-14889
    QColor color = picker.getColor(metricDetail->penColor, this, tr("Choose Metric Color"), QColorDialog::DontUseNativeDialog);

    if (color.isValid()) {
        setButtonIcon(penColor=color);
    }
}

void
EditMetricDetailDialog::setButtonIcon(QColor color)
{

    // create an icon
    QPixmap pix(24, 24);
    QPainter painter(&pix);
    if (color.isValid()) {
    painter.setPen(Qt::gray);
    painter.setBrush(QBrush(color));
    painter.drawRect(0, 0, 24, 24);
    }
    QIcon icon;
    icon.addPixmap(pix);
    curveColor->setIcon(icon);
    curveColor->setContentsMargins(2,2,2,2);
    curveColor->setFixedWidth(34);
}

void
LTMTool::clearFilter()
{
    filenames.clear();
    _amFiltered = false;

    emit filterChanged();
}

void
LTMTool::setFilter(QStringList files)
{
        _amFiltered = true;
        filenames = files;

        emit filterChanged();
} 

DataFilterEdit::DataFilterEdit(QWidget *parent, Context *context)
: QTextEdit(parent), c(0), context(context)
{
    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(checkErrors()));
}

DataFilterEdit::~DataFilterEdit()
{
}

void
DataFilterEdit::checkErrors()
{
    // parse and present errors to user
    DataFilter checker(this, context);
    QStringList errors = checker.check(toPlainText());
    checker.colorSyntax(document(), textCursor().position()); // syntax + error highlighting

    // need to fixup for errors!
    // XXX next commit
}

bool
DataFilterEdit::event(QEvent *e)
{
    // intercept all events
    if (e->type() == QEvent::ToolTip) {
       // XXX error reporting when mouse over error
    }

    // call standard event handler
    return QTextEdit::event(e);
}

void DataFilterEdit::setCompleter(QCompleter *completer)
{
    if (c)
        QObject::disconnect(c, 0, this, 0);

    c = completer;

    if (!c)
        return;

    c->setWidget(this);
    c->setCompletionMode(QCompleter::PopupCompletion);
    c->setCaseSensitivity(Qt::CaseInsensitive);
    QObject::connect(c, SIGNAL(activated(QString)),
                     this, SLOT(insertCompletion(QString)));
}

QCompleter *DataFilterEdit::completer() const
{
    return c;
}

void DataFilterEdit::insertCompletion(const QString& completion)
{
    if (c->widget() != this)
        return;
    QTextCursor tc = textCursor();
    int extra = completion.length() - c->completionPrefix().length();
    tc.movePosition(QTextCursor::Left);
    tc.movePosition(QTextCursor::EndOfWord);
    tc.insertText(completion.right(extra));
    setTextCursor(tc);

    checkErrors();
}

void
DataFilterEdit::setText(const QString &text)
{
    // set text..
    QTextEdit::setText(text);
    checkErrors();
}

QString DataFilterEdit::textUnderCursor() const
{
    QTextCursor tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    return tc.selectedText();
}

void DataFilterEdit::focusInEvent(QFocusEvent *e)
{
    if (c) c->setWidget(this);
    QTextEdit::focusInEvent(e);
}

void DataFilterEdit::keyPressEvent(QKeyEvent *e)
{
    // wait a couple of seconds before checking the changes....
    if (c && c->popup()->isVisible()) {
        // The following keys are forwarded by the completer to the widget
       switch (e->key()) {
       case Qt::Key_Enter:
       case Qt::Key_Return:
       case Qt::Key_Escape:
       case Qt::Key_Tab:
       case Qt::Key_Backtab:
            e->ignore();
            return; // let the completer do default behavior
       default:
           break;
       }
    }

    bool isShortcut = ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_E); // CTRL+E
    if (!c || !isShortcut) // do not process the shortcut when we have a completer
        QTextEdit::keyPressEvent(e);

    // check
    checkErrors();

    const bool ctrlOrShift = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    if (!c || (ctrlOrShift && e->text().isEmpty()))
        return;

    static QString eow("~!@#$%^&*()_+{}|:\"<>?,./;'[]\\-="); // end of word
    bool hasModifier = (e->modifiers() != Qt::NoModifier) && !ctrlOrShift;
    QString completionPrefix = textUnderCursor();

    // are we in a comment ?
    QString line = textCursor().block().text().trimmed();
    for(int i=textCursor().positionInBlock(); i>=0; i--)
        if (line[i]=='#') return;

    if (!isShortcut && (hasModifier || e->text().isEmpty()|| completionPrefix.length() < 1
                      || eow.contains(e->text().right(1)))) {
        c->popup()->hide();
        return;
    }

    if (completionPrefix != c->completionPrefix()) {
        c->setCompletionPrefix(completionPrefix);
        c->popup()->setCurrentIndex(c->completionModel()->index(0, 0));
    }
    QRect cr = cursorRect();
    cr.setWidth(c->popup()->sizeHintForColumn(0)
                + c->popup()->verticalScrollBar()->sizeHint().width());
    c->complete(cr); // popup it up!
}
