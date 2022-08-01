#include "servicesettingsmodule.h"

#include "widgets/widgetmodule.h"

#include <widgets/servicecontrolitems.h>
#include <widgets/settingsgroup.h>
#include <widgets/switchwidget.h>
#include <privacysecuritymodel.h>
#include <privacysecurityworker.h>

#include <DListView>
#include <DStyleHelper>
#include <DStyledItemDelegate>
#include <DSwitchButton>
#include <QBoxLayout>
#include <QScroller>

DCC_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

using namespace DCC_PRIVACY_NAMESPACE;
ServiceSettingsModule::ServiceSettingsModule(DATE& serviceDate, PrivacySecurityModel *model, PrivacySecurityWorker *work, QObject *parent)
    : ModuleObject(serviceDate.name, serviceDate.displayName, QIcon::fromTheme(serviceDate.icon), parent)
    , m_currentServiceDate(serviceDate)
    , m_model(model)
    , m_worker(work)
{
    deactive();
    m_serviceItemDate = m_model->getServiceItem(serviceDate.category);
    setChildType(ModuleObject::Page);
    // 添加标题
    appendChild(new WidgetModule<SwitchWidget>(m_currentServiceDate.name, m_currentServiceDate.displayName, this, &ServiceSettingsModule::initSwitchWidget));

    // 添加无服务
    // 添加文字说明
    ModuleObject *topTipsLabel = new WidgetModule<QLabel>("", "", this, &ServiceSettingsModule::initTopTipsLabel);
    appendChild(topTipsLabel);

    // 添加应用
    m_appsListView = new WidgetModule<DTK_WIDGET_NAMESPACE::DListView>("", "", this, &ServiceSettingsModule::initListView);
    appendChild(m_appsListView);

    // 添加无服务图标
    ModuleObject *noServiceLabel =new WidgetModule<QWidget>("","", this, &ServiceSettingsModule::initNoServiceLabel);
    appendChild(noServiceLabel);
}


ServiceSettingsModule::~ServiceSettingsModule()
{

}

void ServiceSettingsModule::initTopTipsLabel(QLabel *tipsLabel)
{
    tipsLabel->setText(getTopTipsDoc(m_currentServiceDate.category));
    tipsLabel->setVisible(m_serviceItemDate->getServiceAvailable());
    connect(m_serviceItemDate, &ServiceControlItems::serviceAvailableStateChange, tipsLabel, &QLabel::setVisible);
}

void ServiceSettingsModule::initSwitchWidget(DCC_NAMESPACE::SwitchWidget *titleSwitch)
{
    QPixmap pixmap;
    QSize size(42,42);
    QIcon icon = QIcon::fromTheme(m_currentServiceDate.icon);
    pixmap = icon.pixmap(size);
    titleSwitch->setTitle(m_currentServiceDate.name);
    QLabel *iconLabel = new QLabel;
    iconLabel->setPixmap(pixmap);
    iconLabel->setMaximumWidth(50);

    connect(m_serviceItemDate, &ServiceControlItems::serviceSwitchStateChange, titleSwitch, &SwitchWidget::setChecked);
    connect(titleSwitch, &SwitchWidget::checkedChanged, m_worker, [this](bool checkState){
        m_worker->setPermissionEnable(m_serviceItemDate->getServiceGroup(), m_model->getDaemonDefineName(m_currentServiceDate.category), checkState);
    });
    connect(m_serviceItemDate, &ServiceControlItems::serviceAvailableStateChange, titleSwitch, [=](bool serviceAvaiable){
        titleSwitch->switchButton()->setVisible(serviceAvaiable);
    });

    titleSwitch->getMainLayout()->insertWidget(0, iconLabel, Qt::AlignVCenter);
    titleSwitch->setChecked(m_serviceItemDate->getSwitchState());
    titleSwitch->switchButton()->setVisible(m_serviceItemDate->getServiceAvailable());
}

void ServiceSettingsModule::initListView(Dtk::Widget::DListView *timeGrp)
{
    QStandardItemModel *pluginAppsModel = new QStandardItemModel;
    creatPluginAppsView(timeGrp);
    timeGrp->setModel(pluginAppsModel);

    qDebug() << " Get Apps size: " << m_model->getServiceItem(m_currentServiceDate.category)->getServiceApps().size();

    auto updateItemCheckStatus = [pluginAppsModel, timeGrp](const QString& name, const QString& visible) {
        qDebug() << " == pluginAppsModel->rowCount()" << pluginAppsModel->rowCount();
        for (int i = 0; i < pluginAppsModel->rowCount(); ++i) {
            auto item = static_cast<DStandardItem *>(pluginAppsModel->item(i));
            if (item->text() != name || item->actionList(Qt::Edge::RightEdge).size() < 1)
                continue;

            auto action = item->actionList(Qt::Edge::RightEdge).first();
            auto checkstatus = (visible == "1" ? DStyle::SP_IndicatorChecked : DStyle::SP_IndicatorUnchecked);
            auto icon = qobject_cast<DStyle *>(timeGrp->style())->standardIcon(checkstatus);
            action->setIcon(icon);
            timeGrp->update(item->index());
            break;
        }
    };

    auto refreshItemDate = [this , pluginAppsModel, timeGrp, updateItemCheckStatus] () {
        pluginAppsModel->clear();
        for (auto App : m_model->getServiceItem(m_currentServiceDate.category)->getServiceApps()) {
            DStandardItem *item = new DStandardItem(App.m_name);
            item->setFontSize(DFontSizeManager::T8);
            QSize size(16, 16);

            // 图标
            auto leftAction = new DViewItemAction(Qt::AlignVCenter, size, size, true);
            leftAction->setIcon(QIcon::fromTheme("application-x-desktop"));
            item->setActionList(Qt::Edge::LeftEdge, {leftAction});

            auto rightAction = new DViewItemAction(Qt::AlignVCenter, size, size, true);
            bool visible = App.m_enable != "0";
            auto checkstatus = visible ? DStyle::SP_IndicatorChecked : DStyle::SP_IndicatorUnchecked ;
            auto checkIcon = qobject_cast<DStyle *>(timeGrp->style())->standardIcon(checkstatus);
            rightAction->setIcon(checkIcon);
            item->setActionList(Qt::Edge::RightEdge, {rightAction});
            pluginAppsModel->appendRow(item);

            connect(rightAction, &DViewItemAction::triggered, this, [ = ] {
                const QString& checkedChange = (App.m_enable == "0" ? "1" : "0");
                m_worker->setPermissionInfo(App.m_name, m_serviceItemDate->getServiceGroup(), m_model->getDaemonDefineName(m_currentServiceDate.category), checkedChange);
                updateItemCheckStatus(App.m_name, checkedChange);
            });
        }
        m_appsListView->setDisabled(!m_serviceItemDate->getSwitchState());
        m_appsListView->setHiden(!m_serviceItemDate->getServiceAvailable());
    };

    connect(m_serviceItemDate, &ServiceControlItems::serviceSwitchStateChange, timeGrp, &DListView::setEnabled);
    connect(m_serviceItemDate, &ServiceControlItems::permissionInfoChange, timeGrp, [updateItemCheckStatus](const QString& name, const QString& visible){
        updateItemCheckStatus(name, visible);
    });
    connect(m_serviceItemDate, &ServiceControlItems::serviceAvailableStateChange, timeGrp, &DListView::setVisible);
    connect(m_serviceItemDate, &ServiceControlItems::serviceAppsDateChange, timeGrp, [refreshItemDate](){
         refreshItemDate();
    });

    refreshItemDate();
}

void ServiceSettingsModule::initNoServiceLabel(QWidget *noServiceLabel)
{
    connect(m_serviceItemDate, &ServiceControlItems::serviceAvailableStateChange, noServiceLabel, [=](bool serviceAvailable){
        noServiceLabel->setVisible(!serviceAvailable);
    });

    noServiceLabel->setVisible(!m_serviceItemDate->getServiceAvailable());
    QLabel *iconLabel = new QLabel;

    iconLabel->setPixmap(QIcon(":/icons/deepin/builtin/icons/light/dcc_none_light.svg").pixmap(82,82));
    iconLabel->setAlignment(Qt::AlignHCenter);

    QLabel *label = new QLabel;
    label->setText(getNoneTipsDoc(m_currentServiceDate.category));

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(iconLabel);
    vbox->addWidget(label);
    vbox->setAlignment(Qt::AlignHCenter);

    noServiceLabel->setLayout(vbox);
}

void ServiceSettingsModule::creatPluginAppsView(Dtk::Widget::DListView *appsListView)
{
    appsListView->setBackgroundType(DStyledItemDelegate::BackgroundType::ClipCornerBackground);
    appsListView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    appsListView->setSelectionMode(QListView::SelectionMode::NoSelection);
    appsListView->setEditTriggers(DListView::NoEditTriggers);
    appsListView->setFrameShape(DListView::NoFrame);
    appsListView->setViewportMargins(0, 0, 0, 0);
    appsListView->setItemSpacing(1);
    QMargins itemMargins(appsListView->itemMargins());
    itemMargins.setLeft(14);
    appsListView->setItemMargins(itemMargins);
    appsListView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
}

QString ServiceSettingsModule::getTopTipsDoc(ServiceCategory category)
{
    switch (category) {
    case Camera:
        return QString(tr("Apps can access your camera:"));
    case Microphone:
        return QString(tr("Apps can access your microphone:"));
    case UserFolders:
        return QString(tr("Apps can access user folders:"));
    case Calendar:
        return QString(tr("Apps can access Calendar:"));
    case Screenshots:
        return QString(tr("Apps can access Screen Capture:"));
    default:
        break;
    }
    return QString();
}

QString ServiceSettingsModule::getNoneTipsDoc(ServiceCategory category)
{
    switch (category) {
    case Camera:
        return QString(tr("No apps requested access to the camera"));
    case Microphone:
        return QString(tr("No apps requested access to the microphone"));
    case UserFolders:
        return QString(tr("No apps requested access to user folders"));
    case Calendar:
        return QString(tr("No apps requested access to Calendar"));
    case Screenshots:
        return QString(tr("No apps requested access to Screen Capture"));
    default:
        break;
    }
    return QString();
}

