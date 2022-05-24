#include "updateplugin.h"
#include "updatesettingsmodule.h"

#include "updatemodel.h"
#include "updatewidget.h"
#include "updatectrlwidget.h"
#include "updatework.h"
#include "widgets/titlelabel.h"

#include <DFontSizeManager>

#include <widgets/switchwidget.h>

DWIDGET_USE_NAMESPACE

QString UpdatePlugin::name() const
{
    return QStringLiteral("Update");
}

ModuleObject *UpdatePlugin::module()
{
    if (DSysInfo::uosEditionType() == DSysInfo::UosEuler) {
        return nullptr;
    }
    // 一级页面
    UpdateModule *updateInterface = new UpdateModule;
    updateInterface->setChildType(ModuleObject::ChildType::HList);

    // 检查更新
    ModuleObject *moduleUpdate = new ModuleObject(tr("Check for Updates"), tr("Check for Updates"), this);
    moduleUpdate->setChildType(ModuleObject::ChildType::Page);
    checkUpdateModule *checkUpdatePage = new checkUpdateModule(updateInterface->model(), updateInterface->work(), moduleUpdate);
    moduleUpdate->appendChild(checkUpdatePage);
    updateInterface->appendChild(moduleUpdate);

    // 更新设置
    UpdateSettingsModule *moduleUpdateSettings = new UpdateSettingsModule(updateInterface->model(), updateInterface->work(), updateInterface);
    updateInterface->appendChild(moduleUpdateSettings);

    return updateInterface;
}

int UpdatePlugin::location() const
{
    return 13;
}

UpdateModule::UpdateModule(QObject *parent)
    : ModuleObject(tr("update"),tr("update"),tr("update"),QIcon::fromTheme("dcc_nav_update"),parent)
    , m_model(new UpdateModel(this))
    , m_work(new UpdateWorker(m_model, this))
{
    // TODO: 初始化更新小红点处理
    connect(m_model, &UpdateModel::updatablePackagesChanged, this, &UpdateModule::syncUpdatablePackagesChanged);
    m_work->init();
    m_work->preInitialize();
}

UpdateModule::~UpdateModule()
{
    m_model->deleteLater();
    m_work->deleteLater();
}

void UpdateModule::active()
{
    m_work->activate();
    // 相应授权状态 com.deepin.license.Info
//    m_work->requestRefreshLicenseState();

    connect(m_model, &UpdateModel::beginCheckUpdate, m_work, &UpdateWorker::checkForUpdates);
    connect(m_model, &UpdateModel::updateCheckUpdateTime, m_work, &UpdateWorker::refreshLastTimeAndCheckCircle);
    connect(m_model, &UpdateModel::updateNotifyChanged, this, [this](const bool state) {
        qDebug() << " ---- updateNotifyChanged" << state;
        //关闭“自动提醒”，隐藏提示角标
        if (!state) {
            syncUpdatablePackagesChanged(false);
        } else {
            UpdatesStatus status = m_model->status();
            if (status == UpdatesStatus::UpdatesAvailable || status == UpdatesStatus::Updateing || status == UpdatesStatus::Downloading || status == UpdatesStatus::DownloadPaused || status == UpdatesStatus::Downloaded ||
                    status == UpdatesStatus::Installing || status == UpdatesStatus::RecoveryBackingup || status == UpdatesStatus::RecoveryBackingSuccessed || m_model->getUpdatablePackages()) {
                syncUpdatablePackagesChanged(true);
            }
        }
    });
}

void UpdateModule::syncUpdatablePackagesChanged(const bool isUpdatablePackages)
{
    ModuleData *dataRoot = const_cast<ModuleData *>(moduleData());
    dataRoot->Badge = (isUpdatablePackages && m_model->updateNotify());
    this->setModuleData(dataRoot);
}

QWidget *checkUpdateModule::page()
{
    UpdateWidget *updateWidget = new UpdateWidget;
    updateWidget->setModel(m_model, m_worker);
    if (m_model->systemActivation() == UiActiveState::Authorized || m_model->systemActivation() == UiActiveState::TrialAuthorized || m_model->systemActivation() == UiActiveState::AuthorizedLapse) {
        updateWidget->setSystemVersion(m_model->systemVersionInfo());
    }
    connect(updateWidget, &UpdateWidget::requestLastoreHeartBeat, m_worker, &UpdateWorker::onRequestLastoreHeartBeat);

#ifndef DISABLE_ACTIVATOR
    if (m_model->systemActivation() == UiActiveState::Authorized || m_model->systemActivation() == UiActiveState::TrialAuthorized || m_model->systemActivation() == UiActiveState::AuthorizedLapse) {
        updateWidget->setSystemVersion(m_model->systemVersionInfo());
    }
#endif

#ifndef DISABLE_ACTIVATOR
    connect(m_model, &UpdateModel::systemActivationChanged, this, [ = ](UiActiveState systemactivation) {
        if (systemactivation == UiActiveState::Authorized || systemactivation == UiActiveState::TrialAuthorized || systemactivation == UiActiveState::AuthorizedLapse) {
            if (updateWidget)
                updateWidget->setSystemVersion(m_model->systemVersionInfo());
        }
    });
#endif

    connect(updateWidget, &UpdateWidget::requestUpdates, m_worker, &UpdateWorker::distUpgrade);
    connect(updateWidget, &UpdateWidget::requestUpdateCtrl, m_worker, &UpdateWorker::OnDownloadJobCtrl);
    connect(updateWidget, &UpdateWidget::requestOpenAppStroe, m_worker, &UpdateWorker::onRequestOpenAppStore);
    connect(updateWidget, &UpdateWidget::requestFixError, m_worker, &UpdateWorker::onFixError);
    updateWidget->displayUpdateContent(UpdateWidget::UpdateType::UpdateCheck);

    return updateWidget;
}

UpdateTitleModule::UpdateTitleModule(const QString &name, const QString &title, QObject *parent)
    : ModuleObject(parent)
{
    moduleData()->Name = name;
    moduleData()->Description = title;
    moduleData()->ContentText.append(title);
}
QWidget *UpdateTitleModule::page()
{
    TitleLabel *titleLabel = new TitleLabel(moduleData()->Description);
    DFontSizeManager::instance()->bind(titleLabel, DFontSizeManager::T5, QFont::DemiBold); // 设置字体
    return titleLabel;
}

SwitchWidgetModule::SwitchWidgetModule(const QString &name, const QString &title, QObject *parent)
    : ModuleObject(parent)
{
    moduleData()->Name = name;
    moduleData()->Description = title;
    moduleData()->ContentText.append(title);
}

QWidget *SwitchWidgetModule::page()
{
    SwitchWidget *sw_widget = new SwitchWidget(moduleData()->Description);
    return sw_widget;
}

