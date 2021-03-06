#include "dubproject.h"

#include "dubprojectmanagerconstants.h"

#include "dubmanager.h"
#include "dubfile.h"
#include "dubprojectnode.h"
#include "dubbuildconfiguration.h"
#include "dubrunconfiguration.h"
#include "dubconfigparser.h"

#include "dubexception.h"

#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/kit.h>
#include <projectexplorer/target.h>
#include <projectexplorer/buildinfo.h>
#include <coreplugin/icontext.h>
#include <coreplugin/messagemanager.h>
#include <cpptools/cppmodelmanager.h>

#include <qtsupport/customexecutablerunconfiguration.h>

#include <QFileSystemWatcher>

using namespace DubProjectManager;

DubProject::DubProject(DubManager *manager, const QString &filePath)
    : m_manager(manager),
      m_filename(filePath),
      m_rootNode(new DubProjectNode(filePath))
{
    setId(DubProjectManager::Constants::DUBPROJECT_ID);
    setProjectContext(Core::Context(DubProjectManager::Constants::PROJECTCONTEXT));
    setProjectLanguages(Core::Context(ProjectExplorer::Constants::LANG_CXX));

    m_projectName = QFileInfo(filePath).absoluteDir().dirName();
    m_buildDirectory = QFileInfo(m_filename).absoluteDir().absolutePath();

    m_parser = new DubConfigParser(m_buildDirectory);

    m_file = new DubFile(filePath, this);
    m_watcher = new QFileSystemWatcher(this);
    m_watcher->addPath(filePath);

    connect(m_watcher, SIGNAL(fileChanged(QString)), this, SLOT(dubFileChanged(QString)));

    init();

}

DubProject::~DubProject()
{
}

QString DubProject::displayName() const
{
    return m_projectName;
}

Core::IDocument *DubProject::document() const
{
    return m_file;
}

ProjectExplorer::IProjectManager *DubProject::projectManager() const
{
    return m_manager;
}

ProjectExplorer::ProjectNode *DubProject::rootProjectNode() const
{
    return m_rootNode;
}

QStringList DubProject::files(ProjectExplorer::Project::FilesMode fileMode) const
{
    Q_UNUSED(fileMode);
    return m_files;
}

void DubProject::parseConfig()
{
    if (!m_parser->parse()) {
        throw DubException(m_parser->errorString());
    }
    const ConfigurationInfo& s = m_parser->configurationInfo(m_parser->configurationsList().front());
    m_files = s.files();
    m_projectName = m_parser->projectName();
    m_rootNode->setDisplayName(m_projectName);
    m_type = s.targetType() == "executable" ? EXECUTABLE : LIBRARY;

    appendIncludePaths(s);

}

void DubProject::init()
{
    try {
        updateSourceTree();
    }
    catch (const DubException &ex) {
        Core::MessageManager::write(ex.description(), Core::MessageManager::Flash);
    }
    catch (...) {
        Core::MessageManager::write(QLatin1String("Unknown error during loading dub project"), Core::MessageManager::Flash);
    }
}

const QString &DubProject::buildDirectory() const
{
    return m_buildDirectory;
}

const QStringList &DubProject::configurationList() const
{
    return m_parser->configurationsList();
}

const QStringList &DubProject::buildTypesList() const
{
    return m_parser->buildTypesList();
}

const QString &DubProject::sourceTreeConfiguration() const
{
    return m_configuration;
}

const ConfigurationInfo &DubProject::info(const QString conf)
{
    try {
        return m_parser->configurationInfo(conf);
    } catch (...) {
        Core::MessageManager::write(tr("Configuration %1 not found").arg(conf), Core::MessageManager::Flash);
        return m_parser->configurationInfo(m_parser->configurationsList().front());
    }
}

QVariantMap DubProject::toMap() const
{
    QVariantMap map(ProjectExplorer::Project::toMap());
    map.insert(QLatin1String(Constants::S_SOURCE_TREE_CONFIG), m_configuration);
    return map;
}

void DubProject::update()
{
    try {
        updateSourceTree();
    }
    catch (const DubException &ex) {
        Core::MessageManager::write(ex.description(), Core::MessageManager::Flash);
        return;
    }
    catch (...) {
        Core::MessageManager::write(tr("Unknown error during updating source tree"), Core::MessageManager::Flash);
        return;
    }
    emit updated();
}

void DubProject::setSourceTreeConfiguration(const QString &conf)
{
    if (conf != m_configuration && (conf.isEmpty() || m_parser->configurationsList().contains(conf))) {
        buildSourceTree(conf);
    }
}

bool DubProject::fromMap(const QVariantMap &map)
{
    if (!Project::fromMap(map))
        return false;

    bool hasUserFile = activeTarget();
    if (hasUserFile) {
        setSourceTreeConfiguration(map.value(QLatin1String(Constants::S_SOURCE_TREE_CONFIG)).toString());
    } else {
        ProjectExplorer::Kit *defaultKit = ProjectExplorer::KitManager::defaultKit();
        if (defaultKit) {
            ProjectExplorer::Target *t = new ProjectExplorer::Target(this, defaultKit);
            t->updateDefaultBuildConfigurations();
            t->updateDefaultDeployConfigurations();
            t->updateDefaultRunConfigurations();
            addTarget(t);
            setupTargets();
        }
    }
    return true;
}

void DubProject::setupTargets()
{
    ProjectExplorer::Kit* defaultKit = ProjectExplorer::KitManager::defaultKit();
    ProjectExplorer::IBuildConfigurationFactory* factory = ProjectExplorer::IBuildConfigurationFactory::find(defaultKit, m_filename);
    QList<const ProjectExplorer::BuildInfo *> infos;
    ProjectExplorer::BuildInfo *info = new ProjectExplorer::BuildInfo(factory);
    info->buildDirectory = Utils::FileName::fromString(buildDirectory());
    info->kitId = defaultKit->id();
    info->displayName = QString::fromLatin1("all");
    info->supportsShadowBuild = true;
    info->typeName = "Dub Manager";
    infos.push_back(info);
    this->setup(infos);

    foreach (ProjectExplorer::Target* t, this->targets()) {
        DubRunConfiguration* rc =
                new DubRunConfiguration(t, DubRunConfigurationFactory::idFromBuildTarget(m_projectName), this);
        t->addRunConfiguration(rc);
        t->updateDefaultRunConfigurations();
    }
}

void DubProject::buildSourceTree(const QString& conf)
{
    m_configuration = conf;
    if (conf.isEmpty()) {
        m_files.clear();
        foreach (const QString& c, m_parser->configurationsList()) {
            m_files << m_parser->configurationInfo(c).files();
        }
        m_files.removeDuplicates();
    } else {
        m_files = m_parser->configurationInfo(conf).files();
    }

    // build tree
    m_rootNode->clear();
    m_rootNode->addFiles(m_files, 0);
    m_rootNode->addFilePath(m_filename);
    emit fileListChanged();
}

void DubProject::updateSourceTree()
{
    parseConfig();
    buildSourceTree(m_parser->configurationsList().front());
}

void DubProject::appendIncludePaths(const ConfigurationInfo& info)
{
    CppTools::CppModelManager *modelmanager =
            CppTools::CppModelManager::instance();
    if (modelmanager) {
        CppTools::ProjectInfo pinfo = modelmanager->projectInfo(this);
        pinfo.clearProjectParts();

        CppTools::ProjectPart::Ptr part(new CppTools::ProjectPart);
        part->project = this;
        part->displayName = displayName();

        // This explicitly adds -I. to the include paths
        part->projectFile = projectFilePath().toString();
        part->headerPaths.push_back(CppTools::ProjectPart::HeaderPath(projectDirectory().toString(),
                                                                      CppTools::ProjectPart::HeaderPath::IncludePath));
        foreach (const QString &imp, info.importPaths()) {
            part->headerPaths.push_back(CppTools::ProjectPart::HeaderPath(imp, CppTools::ProjectPart::HeaderPath::IncludePath));
        }

        pinfo.appendProjectPart(part);
        pinfo.finish();
        modelmanager->updateProjectInfo(pinfo);
    }
}

void DubProject::dubFileChanged(const QString &filename)
{
    Q_UNUSED(filename);
    init();
}
