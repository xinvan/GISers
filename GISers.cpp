#include "GISers.h"

#include <QDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QGridLayout>
#include <QFont>
#include <QLineEdit>
#include <QToolButton>
#include <QMargins>
// QGis include
#include <qgsvectorlayer.h>
#include <qgsrasterlayer.h>
#include <qgsproject.h>
#include <qgslayertreemodel.h>
#include <qgsapplication.h>
#include <qgslayertreelayer.h>
#include <qgslayertreegroup.h>
#include <qgslayertreeregistrybridge.h>
#include <qgslayertreeviewdefaultactions.h>
#include "qgis_devlayertreeviewmenuprovider.h"

GISers* GISers::sm_instance = 0;

GISers::GISers(QWidget *parent, Qt::WindowFlags flags)
	: QMainWindow(parent, flags)
{
	ui.setupUi(this);

	//! 初始化map canvas, 并装入layout中
	mapCanvas = new QgsMapCanvas();
	mapCanvas->enableAntiAliasing(true);
	mapCanvas->setCanvasColor(QColor(255, 255, 255));

	//! 初始化图层管理器
	m_layerTreeView = new QgsLayerTreeView(this);
	initLayerTreeView();

	//! 布局
	QWidget* centralWidget = this->centralWidget();
	QGridLayout* centralLayout = new QGridLayout(centralWidget);
	centralLayout->addWidget(mapCanvas, 0, 0, 1, 1);

	// connections
	connect(ui.actionAdd_Vector, SIGNAL(triggered()), this, SLOT(addVectorLayers()));
	connect(ui.actionAdd_Raster, SIGNAL(triggered()), this, SLOT(addRasterLayers()));
}

GISers::~GISers()
{

}

void GISers::addVectorLayers()
{
	QString filename = QFileDialog::getOpenFileName(this, tr("open vector"), "", "*.shp");
	QStringList temp = filename.split(QDir::separator());
	QString basename = temp.at(temp.size() - 1);
	QgsVectorLayer* vecLayer = new QgsVectorLayer(filename, basename, "ogr");
	if (!vecLayer->isValid())
	{
		QMessageBox::critical(this, "error", "layer is invalid");
		return;
	}
	mapCanvasLayerSet.append(vecLayer);
	mapCanvas->setExtent(vecLayer->extent());
	mapCanvas->setLayers(mapCanvasLayerSet);
	mapCanvas->setVisible(true);
	mapCanvas->freeze(false);
	mapCanvas->refresh();
}

void GISers::addRasterLayers()
{
	QString filename = QFileDialog::getOpenFileName(this, tr("open vector"), "", "*.tif");
	QStringList temp = filename.split(QDir::separator());
	QString basename = temp.at(temp.size() - 1);
	QgsRasterLayer* rasterLayer = new QgsRasterLayer(filename, basename, "gdal");
	if (!rasterLayer->isValid())
	{
		QMessageBox::critical(this, "error", "layer is invalid");
		return;
	}

	mapCanvasLayerSet.append(rasterLayer);
	mapCanvas->setExtent(rasterLayer->extent());
	mapCanvas->setLayers(mapCanvasLayerSet);
	mapCanvas->setVisible(true);
	mapCanvas->freeze(false);
	mapCanvas->refresh();
}

void GISers::initLayerTreeView()
{
	QgsLayerTreeModel* model = new QgsLayerTreeModel(QgsProject::instance()->layerTreeRoot(), this);
	model->setFlag(QgsLayerTreeModel::AllowNodeRename);
	model->setFlag(QgsLayerTreeModel::AllowNodeReorder);
	model->setFlag(QgsLayerTreeModel::AllowNodeChangeVisibility);
	model->setFlag(QgsLayerTreeModel::ShowLegendAsTree);
	model->setAutoCollapseLegendNodes(10);

	m_layerTreeView->setModel(model);
	// 添加右键菜单
	m_layerTreeView->setMenuProvider(new qgis_devLayerTreeViewMenuProvider(m_layerTreeView, mapCanvas));

	connect(QgsProject::instance()->layerTreeRegistryBridge(), SIGNAL(addedLayersToLayerTree(QList<QgsMapLayer*>)),
		this, SLOT(autoSelectAddedLayer(QList<QgsMapLayer*>)));
	
	// 设置这个路径是为了获取图标文件
	QString iconDir = "../images/themes/default/";

	// add group tool button
	QToolButton * btnAddGroup = new QToolButton();
	btnAddGroup->setAutoRaise(true);
	btnAddGroup->setIcon(QIcon(iconDir + "mActionAdd.png"));

	btnAddGroup->setToolTip(tr("Add Group"));
	connect(btnAddGroup, SIGNAL(clicked()), m_layerTreeView->defaultActions(), SLOT(addGroup()));

	// expand / collapse tool buttons
	QToolButton* btnExpandAll = new QToolButton();
	btnExpandAll->setAutoRaise(true);
	btnExpandAll->setIcon(QIcon(iconDir + "mActionExpandTree.png"));
	btnExpandAll->setToolTip(tr("Expand All"));
	connect(btnExpandAll, SIGNAL(clicked()), m_layerTreeView, SLOT(expandAll()));

	QToolButton* btnCollapseAll = new QToolButton();
	btnCollapseAll->setAutoRaise(true);
	btnCollapseAll->setIcon(QIcon(iconDir + "mActionCollapseTree.png"));
	btnCollapseAll->setToolTip(tr("Collapse All"));
	connect(btnCollapseAll, SIGNAL(clicked()), m_layerTreeView, SLOT(collapseAll()));

	// remove item button
	QToolButton* btnRemoveItem = new QToolButton();
	// btnRemoveItem->setDefaultAction( this->m_actionRemoveLayer );
	btnRemoveItem->setAutoRaise(true);


	// 按钮布局
	QHBoxLayout* toolbarLayout = new QHBoxLayout();
	toolbarLayout->setContentsMargins(QMargins(5, 0, 5, 0));
	toolbarLayout->addWidget(btnAddGroup);
	toolbarLayout->addWidget(btnCollapseAll);
	toolbarLayout->addWidget(btnExpandAll);
	toolbarLayout->addWidget(btnRemoveItem);
	toolbarLayout->addStretch();

	QVBoxLayout* vboxLayout = new QVBoxLayout();
	vboxLayout->setMargin(0);
	vboxLayout->addLayout(toolbarLayout);
	vboxLayout->addWidget(m_layerTreeView);

	// 装进dock widget中
	m_layerTreeDock = new QDockWidget(tr("Layers"), this);
	m_layerTreeDock->setObjectName("Layers");
	m_layerTreeDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	QWidget* w = new QWidget();
	w->setLayout(vboxLayout);
	m_layerTreeDock->setWidget(w);
	addDockWidget(Qt::LeftDockWidgetArea, m_layerTreeDock);

	

	
	// 连接地图画布和图层管理器
	m_layerTreeCanvasBridge = new QgsLayerTreeMapCanvasBridge(QgsProject::instance()->layerTreeRoot(), mapCanvas, this);
	connect(QgsProject::instance(), SIGNAL(writeProject(QDomDocument&)),m_layerTreeCanvasBridge, SLOT(writeProject(QDomDocument&)));
	connect(QgsProject::instance(), SIGNAL(readProject(QDomDocument)),m_layerTreeCanvasBridge, SLOT(readProject(QDomDocument)));



}

void GISers::createStatusBar()
{
	statusBar()->setStyleSheet("QStatusBar::item {border: none;}");

	//! 添加进度条
	m_progressBar = new QProgressBar(statusBar());
	m_progressBar->setObjectName("m_progressBar");
	m_progressBar->setMaximum(100);
	m_progressBar->hide();
	statusBar()->addPermanentWidget(m_progressBar, 1);
	connect(mapCanvas, SIGNAL(renderStarting()), this, SLOT(canvasRefreshStarted()));
	connect(mapCanvas, SIGNAL(mapCanvasRefreshed()), this, SLOT(canvasRefreshFinished()));

	QFont myFont("Arial", 9);
	statusBar()->setFont(myFont);

	//! 添加坐标显示标签
	m_coordsLabel = new QLabel(QString(), statusBar());
	m_coordsLabel->setObjectName("m_coordsLabel");
	m_coordsLabel->setFont(myFont);
	m_coordsLabel->setMinimumWidth(10);
	m_coordsLabel->setMargin(3);
	m_coordsLabel->setAlignment(Qt::AlignCenter);
	m_coordsLabel->setFrameStyle(QFrame::NoFrame);
	m_coordsLabel->setText(tr("Coordinate:"));
	m_coordsLabel->setToolTip(tr("Current map coordinate"));
	statusBar()->addPermanentWidget(m_coordsLabel, 0);

	//! 坐标编辑标签
	m_coordsEdit = new QLineEdit(QString(), statusBar());
	m_coordsEdit->setObjectName("m_coordsEdit");
	m_coordsEdit->setFont(myFont);
	m_coordsEdit->setMinimumWidth(10);
	m_coordsEdit->setMaximumWidth(300);
	m_coordsEdit->setContentsMargins(0, 0, 0, 0);
	m_coordsEdit->setAlignment(Qt::AlignCenter);
	statusBar()->addPermanentWidget(m_coordsEdit, 0);
	connect(m_coordsEdit, SIGNAL(returnPressed()), this, SLOT(userCenter()));

	//! 比例尺标签
	m_scaleLabel = new QLabel(QString(), statusBar());
	m_scaleLabel->setObjectName("m_scaleLabel");
	m_scaleLabel->setFont(myFont);
	m_scaleLabel->setMinimumWidth(10);
	m_scaleLabel->setMargin(3);
	m_scaleLabel->setAlignment(Qt::AlignCenter);
	m_scaleLabel->setFrameStyle(QFrame::NoFrame);
	m_scaleLabel->setText(tr("Scale"));
	m_scaleLabel->setToolTip(tr("Current map scale"));
	statusBar()->addPermanentWidget(m_scaleLabel, 0);

	m_scaleEdit = new QgsScaleComboBox(statusBar());
	m_scaleEdit->setObjectName("m_scaleEdit");
	m_scaleEdit->setFont(myFont);
	m_scaleEdit->setMinimumWidth(10);
	m_scaleEdit->setMaximumWidth(100);
	m_scaleEdit->setContentsMargins(0, 0, 0, 0);
	m_scaleEdit->setToolTip(tr("Current map scale (formatted as x:y)"));
	statusBar()->addPermanentWidget(m_scaleEdit, 0);
	connect(m_scaleEdit, SIGNAL(scaleChanged()), this, SLOT(userScale()));

}

void GISers::autoSelectAddedLayer(QList<QgsMapLayer*> layers)
{/*
	if (layers.count())
	{
		QgsLayerTreeLayer* nodeLayer = QgsProject::instance()->layerTreeRoot()->findLayer(layers[0]->id()) ;

		if (!nodeLayer)
		{
			return;
		}

		QModelIndex index = m_layerTreeView->layerTreeModel()->node2index(nodeLayer);
		m_layerTreeView->setCurrentIndex(index);
	}*/
}

void GISers::addDockWidget(Qt::DockWidgetArea area, QDockWidget* dockwidget)
{
	QMainWindow::addDockWidget(area, dockwidget);
	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

	dockwidget->show();
	mapCanvas->refresh();
}