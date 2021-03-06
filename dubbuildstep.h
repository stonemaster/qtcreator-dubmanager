#ifndef DUBBUILDSTEP_H
#define DUBBUILDSTEP_H

#include <projectexplorer/abstractprocessstep.h>
#include <projectexplorer/buildstep.h>
#include <projectexplorer/ioutputparser.h>
#include <projectexplorer/task.h>

QT_FORWARD_DECLARE_CLASS(QLineEdit)
QT_FORWARD_DECLARE_CLASS(QComboBox)

namespace DubProjectManager {

class DubProject;

class DubBuildStep : public ProjectExplorer::AbstractProcessStep
{
    Q_OBJECT
public:
    explicit DubBuildStep(ProjectExplorer::BuildStepList *bsl, const Core::Id id);
    explicit DubBuildStep(ProjectExplorer::BuildStepList *bsl);

    // pure ProjectExplorer::AbstractProcessStep (BuildStep)

    virtual bool init();

    virtual void run(QFutureInterface<bool> &fi);

    virtual ProjectExplorer::BuildStepConfigWidget *createConfigWidget();

    // others
    
    const QString &additionalArguments() const;
    const QString &configuration() const;
    const QString &package() const;
    const QString &buildType() const;

    QString commandString() const;
    QString command() const;


    virtual QVariantMap toMap() const;
    virtual bool fromMap(const QVariantMap &map);

    const DubProject *dubProject() const;

protected:

signals:

    void updated();
    
public slots:

    void updateBuildType(const QString& type);
    void updateConfiguration(const QString& conf);
    void updateAdditionalArguments(const QString& args);
    void updatePackage(const QString& package);

private:
    void construct();

    DubProject* m_project;

    QString m_package;
    QString m_additionalArguments;
    QString m_buildType;
    QString m_configuration;

    QString generateArguments() const;
};

class DubBuildStepFactory : public ProjectExplorer::IBuildStepFactory
{
    Q_OBJECT

public:
    explicit DubBuildStepFactory(QObject *parent = 0);
    virtual ~DubBuildStepFactory();

    // pure ProjectExplorer::IBuildStepFactory
    bool canCreate(ProjectExplorer::BuildStepList *parent, const Core::Id id) const;
    ProjectExplorer::BuildStep *create(ProjectExplorer::BuildStepList *parent, const Core::Id id);
    bool canClone(ProjectExplorer::BuildStepList *parent, ProjectExplorer::BuildStep *source) const;
    ProjectExplorer::BuildStep *clone(ProjectExplorer::BuildStepList *parent, ProjectExplorer::BuildStep *source);
    bool canRestore(ProjectExplorer::BuildStepList *parent, const QVariantMap &map) const;
    ProjectExplorer::BuildStep *restore(ProjectExplorer::BuildStepList *parent, const QVariantMap &map);

    QList<Core::Id> availableCreationIds(ProjectExplorer::BuildStepList *bc) const;
    QString displayNameForId(const Core::Id id) const;
private:
    bool canHandle(ProjectExplorer::BuildStepList *parent) const;
};

class DubBuildStepConfigWidget : public ProjectExplorer::BuildStepConfigWidget
{
    Q_OBJECT
public:
    explicit DubBuildStepConfigWidget(DubBuildStep* step);

    // pure ProjectExplorer::BuildStepConfigWidget

    virtual QString summaryText() const;
    virtual QString displayName() const;

    // others

public slots:
    void update();

private:
    DubBuildStep *m_step;
    QComboBox *m_configuration;
    QLineEdit *m_package;
    QLineEdit *m_additionalArguments;
    QComboBox *m_buildTargetsList;
};

class DubOutputDmdParser : public ProjectExplorer::IOutputParser
{
    Q_OBJECT

public:
    explicit DubOutputDmdParser();
    void stdError(const QString &line);

protected:
    void doFlush();

private:
    QVector<ProjectExplorer::Task> m_tasks;
    QRegExp m_commonDmdError;

    static const int MAX_TASKS = 10;
};

} // namespace DubProjectManager

#endif // DUBBUILDSTEP_H
