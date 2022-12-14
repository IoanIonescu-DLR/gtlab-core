/* GTlab - Gas Turbine laboratory
 * Source File:
 * copyright 2009-2015 by DLR
 *
 *  Created on: 11.08.2015
 *  Author: Stanislaus Reitenbach (AT-TW)
 *  Tel.: +49 2203 601 2907
 */

#include "gt_moduleloader.h"
#include "gt_moduleinterface.h"
#include "gt_datamodelinterface.h"
#include "gt_objectfactory.h"
#include "gt_logging.h"
#include "gt_algorithms.h"
#include "gt_versionnumber.h"
#include "internal/gt_moduleupgrader.h"
#include "internal/gt_sharedfunctionhandler.h"
#include "internal/gt_commandlinefunctionhandler.h"

#include <QDebug>
#include <QDir>
#include <QPluginLoader>
#include <QDirIterator>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QDomElement>

#include "gt_algorithms.h"
#include "gt_utilities.h"
#include "gt_qtutilities.h"

namespace
{

class ModuleMetaData
{
public:

    explicit ModuleMetaData(const QString& loc)
       : m_libraryLocation(loc)
    {}

    /**
     * Loads the meta data from the module json file
     */
    void readFromJson(const QJsonObject& json);

    const QString& location() const noexcept
    {
        return m_libraryLocation;
    }

    const QString& moduleId() const noexcept
    {
        return m_id;
    }

    /**
     * @return A list of modules (modulename, version) of which
     * this module depends.
     */
    const std::vector<std::pair<QString, GtVersionNumber>>&
    directDependencies() const noexcept
    {
        return m_deps;
    }

    /// Returns a list of modules that allow to suppress this module
    const QStringList& suppressorModules() const noexcept
    {
        return m_suppression;
    }

    const QMap<QString, QString>& environmentVars() const noexcept
    {
        return m_envVars;
    }

private:
    QVariantList
    metaArray(const QJsonObject& metaData, const QString& id)
    {
        return metaData.value(id).toArray().toVariantList();
    }

    QString m_id;
    QString m_libraryLocation;
    std::vector<std::pair<QString, GtVersionNumber>> m_deps;
    QMap<QString, QString> m_envVars;
    QStringList m_suppression;
};


using ModuleMetaMap = std::map<QString, ModuleMetaData>;
ModuleMetaMap loadModuleMeta();

} // namespace

class GtModuleLoader::Impl
{
public:
    /// Successfully loaded plugins
    QMap<QString, GtModuleInterface*> m_plugins;

    /// Mapping of suppressed plugins to their suppressors
    QMap<QString, QSet<QString>> m_suppressedPlugins;

    const std::map<QString, ModuleMetaData> m_metaData{loadModuleMeta()};

    /// Modules initialized indicator.
    bool m_modulesInitialized{false};

    /**
     * @brief performLoading
     *
     *        Implementeds the actual loading logic
     *
     * @param modulesToLoad Modules that need to be loaded
     * @param metaMap Map to the module meta data
     * @return
     */
    bool performLoading(GtModuleLoader& moduleLoader,
                        const QStringList& modulesToLoad,
                        const ModuleMetaMap &metaMap);
    /**
     * @brief checkDependency
     * The dependencies of the module are given by a list and are compared
     * to the modules which are already loaded by the program
     * If a needed dependecy is not loaded in as a plugin in the
     * program the function will return false, else true.
     * The function will also return false if the needed module is available
     * but the version is older than defined in the dependency
     *
     * @param meta The module meta data
     * @return true if all dependencies are ok
     */
    bool dependenciesOkay(const ModuleMetaData& meta);

    /**
     * @brief printDependencies
     */
    void printDependencies(const ModuleMetaData& meta);

    /**
     * @brief Checks whether the module with the given moduleId is suppressed
     * by another module. If the module is suppressed by another module,
     * it checks if this suppression is allowed. Returns true if there is a
     * valid suppression, otherwise returns false.
     * @param m The meta data of the module
     * @return True if there is a valid suppression, otherwise returns false.
     */
    bool isSuppressed(const ModuleMetaData& m) const;

    /**
     * @brief Returns the list of all modules that can be loaded
     * @return
     */
    QStringList getAllLoadableModuleIds() const
    {
        QStringList moduleIds;
        std::transform(m_metaData.begin(), m_metaData.end(),
           std::back_inserter(moduleIds),
           [](const std::pair<QString, ModuleMetaData>& metaEntry)
           {
               return metaEntry.second.moduleId();
           });
        return moduleIds;

    }
};

GtModuleLoader::GtModuleLoader() :
    m_pimpl{std::make_unique<GtModuleLoader::Impl>()}
{ }

GtModuleLoader::~GtModuleLoader() = default;

namespace
{

/**
 * @brief Returns application roaming path.
 * @return Application roaming path.
 */
QString roamingPath()
{
#ifdef _WIN32
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
    return QStandardPaths::writableLocation(
               QStandardPaths::GenericConfigLocation);
}

QDir getModuleDirectory()
{
#ifndef Q_OS_ANDROID
    QString path = QCoreApplication::applicationDirPath() +
                   QDir::separator() + QStringLiteral("modules");
#else
    QString path = QCoreApplication::applicationDirPath();
#endif

    QDir modulesDir(path);
#ifdef Q_OS_WIN
    modulesDir.setNameFilters(QStringList() << QStringLiteral("*.dll"));
#endif

    return modulesDir;
}

QStringList getModuleFilenames()
{
    auto modulesDir = getModuleDirectory();

    if (!modulesDir.exists())
    {
        return {};
    }

    // file names in dir
    auto files = modulesDir.entryList(QDir::Files);

    // convert to absolute file names
    std::transform(files.begin(), files.end(), files.begin(),
                   [&modulesDir](const QString& localFileName) {
        return modulesDir.absoluteFilePath(localFileName);
    });

    return files;
}

QStringList getModulesToExclude()
{
    // initialize module blacklist
    QFile excludeFile(qApp->applicationDirPath() + QDir::separator() +
                      QStringLiteral("_exclude.json"));

    if (!excludeFile.exists())
    {
        return {};
    }

    gtDebug().medium() << "Module exclude file found!";

    if (!excludeFile.open(QIODevice::ReadOnly))
    {
        gtWarning() << QObject::tr("Failed to open the module exclude file!");
        return {};
    }

    QJsonDocument doc(QJsonDocument::fromJson(excludeFile.readAll()));

    excludeFile.close();

    if (doc.isNull())
    {
        gtWarning() << QObject::tr("Failed to parse the module exclude file!");
        return {};
    }

    QJsonObject json = doc.object();

    QVariantList exModList =
            json.value(QStringLiteral("modules")).toArray().toVariantList();

    QStringList excludeList;

    for (const QVariant& exMod : qAsConst(exModList))
    {
        QVariantMap modItem = exMod.toMap();
        const QString name =
            modItem.value(QStringLiteral("id")).toString();

        excludeList << name;

        gtWarning().medium()
                << QObject::tr("Excluding module '%1'!").arg(name);
    }

    return excludeList;
}

/**
 * @brief Take care to log crashed modules
 */
class CrashedModulesLog
{
public:
    CrashedModulesLog() :
        settings(iniFile(), QSettings::IniFormat)
    {
        crashed_mods = settings.value(QStringLiteral("loading_crashed"))
                           .toStringList();
    }

    void push(const QString& moduleFile)
    {
        crashed_mods << moduleFile;
        sync();
    }

    /**
     * @brief Returns a list of crashed modules on last start
     */
    QStringList crashedModules() const
    {
        return crashed_mods;
    }

    void pop()
    {
        crashed_mods.removeLast();
        sync();
    }

    static QString iniFile()
    {
        return roamingPath() + QDir::separator() +
               QStringLiteral("last_run.ini");
    }

    /**
     * Temporarily store the current module. If the app crashes,
     * this will be persitently stored as crashed
     */
    auto makeSnapshot(const QString& currentModuleLocation)
    {
        push(currentModuleLocation);

        return gt::finally([this](){
            pop();
        });
    }

private:
    void sync()
    {
        settings.setValue(QStringLiteral("loading_crashed"), crashed_mods);
        settings.sync();
    }

    QSettings settings;
    QStringList crashed_mods;

};

/**
 * @brief Filters a module meta map according the the function keepModule
 * @param modules Input map of modules
 * @param keepModule Function of signature (bool*)(const ModuleMetaData&)
 * @return Output map of filtered modules
 */
template <typename FilterFun>
ModuleMetaMap filterModules(const ModuleMetaMap& modules, FilterFun keepModule)
{
    ModuleMetaMap result;
    for (const auto& entry : modules)
    {
        const auto& moduleMeta = entry.second;
        if (keepModule(moduleMeta)) {
            result.insert(entry);
        }
    }

    return result;
}

/**
 * @brief Loads the meta data of a single module
 * @param moduleFileName The path to the plugin
 * @return
 */
ModuleMetaData loadModuleMeta(const QString& moduleFileName)
{
    assert(QFile(moduleFileName).exists());

    // load plugin from entry
    QPluginLoader loader(moduleFileName);

    ModuleMetaData meta(moduleFileName);
    meta.readFromJson(loader.metaData());

    return meta;
}

ModuleMetaMap loadModuleMeta()
{
    std::map<QString, ModuleMetaData> metaData;

    const auto moduleFiles = getModuleFilenames();
    for (const QString& moduleFile : moduleFiles)
    {
        const auto meta = loadModuleMeta(moduleFile);
        metaData.insert(std::make_pair(meta.moduleId(), meta));
    }

    const auto crashed_mods = CrashedModulesLog().crashedModules();

    // Remove all modules, that have been crashed earlies
    metaData = filterModules(metaData, [&crashed_mods](const ModuleMetaData& m) {
        if (crashed_mods.contains(m.location()))
        {
            gtWarning() << QObject::tr("'%1' loading skipped (last run crash)")
                           .arg(m.moduleId());
            return false;
        }
        return true;
    });

    // Remove all modules, that should be exluded
    const auto excludeList = getModulesToExclude();
    metaData = filterModules(metaData,[&excludeList](const ModuleMetaData& m) {
        if (excludeList.contains(m.moduleId()))
        {
            gtWarning() << QObject::tr("'%1' loading excluded by exclude list")
                           .arg(m.moduleId());
            return false;
        }
        return true;
    });

    return metaData;
}

} // namespace

bool
GtModuleLoader::loadSingleModule(const QString& moduleLocation)
{
    if (!QFile(moduleLocation).exists())
    {
        gtError() << QObject::tr("Cannot load module. "
                                 "Module File '%1' does not exists.")
                         .arg(moduleLocation);
        return false;
    }

    const auto moduleMeta = loadModuleMeta(moduleLocation);
    if (moduleMeta.moduleId().isEmpty())
    {
        gtError() << QObject::tr("Cannot load module. "
                                 "File '%1' is not a module.")
                         .arg(moduleLocation);
        return false;
    }

    const QStringList modulesToLoad{moduleMeta.moduleId()};

    // the meta data from the module directory
    auto moduleMetaMap = m_pimpl->m_metaData;

    // replace possibly existing module by the current module
    const auto it = moduleMetaMap.find(moduleMeta.moduleId());
    if (it != moduleMetaMap.end())
    {
        // module already exist in metadata, replace
        it->second = moduleMeta;
    }
    else
    {
        moduleMetaMap.insert(std::make_pair(moduleMeta.moduleId(), moduleMeta));
    }

    if (!m_pimpl->performLoading(*this, modulesToLoad, moduleMetaMap))
    {
        gtWarning() << QObject::tr("Could not resolve following "
                                   "plugin dependencies:");

        for (const auto& entry : moduleMetaMap)
        {
            m_pimpl->printDependencies(entry.second);
        }

        return false;
    }

    return true;
}

void
GtModuleLoader::load()
{
    auto allModulesIds = m_pimpl->getAllLoadableModuleIds();
    auto moduleMetaMap = m_pimpl->m_metaData;

    if (!m_pimpl->performLoading(*this, allModulesIds, moduleMetaMap))
    {
        gtWarning() << QObject::tr("Could not resolve following "
                                   "plugin dependencies:");

        for (const auto&entry : qAsConst(moduleMetaMap))
        {
            m_pimpl->printDependencies(entry.second);
        }
    }
}

QMap<QString, QString>
GtModuleLoader::moduleEnvironmentVars()
{
    auto moduleMeta = loadModuleMeta();

    QMap<QString, QString> retval;
    for (const auto& mod : moduleMeta)
    {
        retval.insert(mod.second.environmentVars());
    }

    return retval;
}

QStringList
GtModuleLoader::moduleIds() const
{
    return m_pimpl->m_plugins.keys();
}

QStringList
GtModuleLoader::moduleDatamodelInterfaceIds() const
{
    QStringList retval;
    gt::for_each_key(m_pimpl->m_plugins, [&](const QString& e){
        GtDatamodelInterface* dmi =
               dynamic_cast<GtDatamodelInterface*>(m_pimpl->m_plugins.value(e));

        if (dmi && dmi->standAlone())
        {
            retval << e;
        }
    });

    return retval;
}

GtVersionNumber
GtModuleLoader::moduleVersion(const QString& id) const
{
    if (m_pimpl->m_plugins.contains(id))
    {
        return m_pimpl->m_plugins.value(id)->version();
    }

    return GtVersionNumber();
}

QString
GtModuleLoader::moduleDescription(const QString& id) const
{
    if (m_pimpl->m_plugins.contains(id))
    {
        return m_pimpl->m_plugins.value(id)->description();
    }

    return QString();
}

QString
GtModuleLoader::moduleAuthor(const QString& id) const
{
    if (m_pimpl->m_plugins.contains(id))
    {
        return m_pimpl->m_plugins.value(id)->metaInformation().author;
    }

    return QString();
}

QString
GtModuleLoader::moduleContact(const QString& id) const
{
    if (m_pimpl->m_plugins.contains(id))
    {
        return m_pimpl->m_plugins.value(id)->metaInformation().authorContact;
    }

    return QString();
}

QString
GtModuleLoader::moduleLicence(const QString& id) const
{
    if (m_pimpl->m_plugins.contains(id))
    {
        return m_pimpl->m_plugins.value(id)->metaInformation().licenseShort;
    }

    return QString();
}
QString
GtModuleLoader::modulePackageId(const QString& id) const
{
    if (m_pimpl->m_plugins.contains(id))
    {
        GtDatamodelInterface* dmi =
                dynamic_cast<GtDatamodelInterface*>(m_pimpl->m_plugins.value(id));

        if (dmi)
        {
            return dmi->package().className();
        }
    }

    return {};
}

void
GtModuleLoader::initModules()
{
    if (m_pimpl->m_modulesInitialized)
    {
        gtWarning() << QObject::tr("Modules already initialized!");
        return;
    }

    for (auto const& value : qAsConst(m_pimpl->m_plugins))
    {
        value->init();
    }

    m_pimpl->m_modulesInitialized = true;
}

void
GtModuleLoader::addSuppression(const QString& suppressorModuleId,
        const QString& suppressedModuleId)
{
    m_pimpl->m_suppressedPlugins[suppressedModuleId] << suppressorModuleId;
}


bool
GtModuleLoader::check(GtModuleInterface* plugin) const
{
    if (m_pimpl->m_plugins.contains(plugin->ident()))
    {
        return false;
    }

    GtDatamodelInterface* dmp = dynamic_cast<GtDatamodelInterface*>(plugin);

    // contains dynamic linked datamodel classes
    if (dmp)
    {
        if (!gtObjectFactory->invokable(dmp->package()))
        {
            return false;
        }
        if (gtObjectFactory->containsDuplicates(dmp->data()))
        {
            return false;
        }

        if (!gtObjectFactory->allInvokable(dmp->data()))
        {
            return false;
        }
    }

    return true;
}

void
GtModuleLoader::insert(GtModuleInterface* plugin)
{
    m_pimpl->m_plugins.insert(plugin->ident(), plugin);

    // register converter funcs
    foreach (const auto& r, plugin->upgradeRoutines())
    {
      gt::detail::GtModuleUpgrader::instance()
            .registerModuleConverter(plugin->ident(), r.target, r.f);
    }

    // register all interface functions of the module
    foreach(const auto& sharedFunction, plugin->sharedFunctions())
    {
        gt::interface::detail::registerFunction(plugin->ident(),
                                                      sharedFunction);
    }

    // register all commandline functions of the module
    foreach(const auto& commandLineFunction, plugin->commandLineFunctions())
    {
        gt::commandline::registerFunction(commandLineFunction);
    }

    GtDatamodelInterface* dmp = dynamic_cast<GtDatamodelInterface*>(plugin);

    // contains dynamic linked datamodel classes
    if (dmp)
    {
        gtObjectFactory->registerClasses(dmp->data());
        gtObjectFactory->registerClass(dmp->package());
    }
}

void
createAdjacencyMatrixImpl(const QStringList& modulesToLoad,
                          const ModuleMetaMap& allModules,
                          std::map<QString, QStringList>& matrix)
{
    for (const auto& moduleId : modulesToLoad)
    {
        if (matrix.find(moduleId) != matrix.end())
        {
            // continue, module is already in matrix, stop recursion
            continue;
        }

        // The module is not yet in the matrix, initialize with an empty list
        matrix[moduleId] = QStringList{};

        auto moduleIt = allModules.find(moduleId);
        if (moduleIt == allModules.end())
        {
            // dependency not found, skip it
            continue;
        }

        // Add dependencies to matrix
        for (const auto& dep : moduleIt->second.directDependencies())
        {
            matrix[moduleId].push_back(dep.first);
        }

        // recurse into dependencies
        createAdjacencyMatrixImpl(matrix[moduleId], allModules, matrix);
    }
}

/**
 * @brief Creates a map of all modules that need to be loaded,
 *        with key=moduleId and value=ModuleDependencies
 * @param modulesToLoad The list of modules to be included
 * @param allModules    The map of metadata of all modules (to query dependencies)
 * @return
 */
std::map<QString, QStringList>
createAdjacencyMatrix(const QStringList& modulesToLoad,
                      const ModuleMetaMap& allModules)
{
    std::map<QString, QStringList> adjMatrix;
    createAdjacencyMatrixImpl(modulesToLoad, allModules, adjMatrix);
    return adjMatrix;
}

/**
 * @brief Solves, which modules need to be loaded and returns the correct
 *        order of loading
 *
 * @param modulesIdsToLoad The ids of all modules that should be loaded
 * @param metaMap The map of module metadata whoch provides dependency
 *                information.
 *
 * @returns All resolved modules in the correct order, that need to be loaded
 */
QStringList
getSortedModulesToLoad(const QStringList& modulesIdsToLoad,
                       const ModuleMetaMap& metaMap)
{
    // create adjacency matrix
    const auto moduleMatrix = createAdjacencyMatrix(modulesIdsToLoad, metaMap);

    // sort modules in the correct order of dependencies
    auto sortedModuleIds = gt::topo_sort(moduleMatrix);
    std::reverse(std::begin(sortedModuleIds), std::end(sortedModuleIds));

    // Only include modules that are actually found in metadata
    auto sortedModuleIdFiltered = QStringList{};
    std::copy_if(sortedModuleIds.begin(), sortedModuleIds.end(),
                 std::back_inserter(sortedModuleIdFiltered),
                 [&metaData = metaMap](const QString& moduleId){
        return metaData.find(moduleId) != metaData.end();
    });

    return sortedModuleIdFiltered;
}

bool
GtModuleLoader::Impl::performLoading(GtModuleLoader& moduleLoader,
                                 const QStringList& moduleIds,
                                 const ModuleMetaMap& metaMap)
{
    // initialize loading fail log
    CrashedModulesLog crashLog;

    auto sortedModuleIds = getSortedModulesToLoad(moduleIds, metaMap);

    bool allLoaded = true;

    // loading procedure
    for (const auto& currentModuleId : qAsConst(sortedModuleIds))
    {
        auto moduleIt = metaMap.find(currentModuleId);
        assert(moduleIt != metaMap.end());

        const ModuleMetaData& moduleMeta = moduleIt->second;

        if (isSuppressed(moduleMeta) || !dependenciesOkay(moduleMeta))
        {
            allLoaded = false;
            continue;
        }

        // store temporary module information in loading fail log
        auto _ = crashLog.makeSnapshot(moduleMeta.location());

        // load plugin from entry
        QPluginLoader loader(moduleMeta.location());
        std::unique_ptr<QObject> plugin(loader.instance());

        // check plugin object
        if (!plugin)
        {
            // could not recreate plugin
            gtError() << loader.errorString();
            allLoaded = false;
            continue;
        }

        gtDebug().medium().nospace()
                << QObject::tr("loading ") << moduleMeta.location() << "...";

        // check that plugin is a GTlab module
        auto module =
            gt::unique_qobject_cast<GtModuleInterface>(std::move(plugin));

        if (module && moduleLoader.check(module.get()))
        {
            moduleLoader.insert(module.get());
            module.release()->onLoad();
        }
        else
        {
            allLoaded = false;
        }
    }

    return allLoaded;
}

bool
GtModuleLoader::Impl::dependenciesOkay(const ModuleMetaData& meta)
{
    if (meta.directDependencies().empty())
    {
        return true;
    }

    for (const auto& dep : meta.directDependencies())
    {
        const QString name = dep.first;
        const GtVersionNumber version = dep.second;

        // check dependency
        if (!m_plugins.contains(name))
        {
            gtError() << QObject::tr("Cannot load module '%1' due to missing "
                                     "dependency '%2'")
                         .arg(meta.moduleId(), name);
            return false;
        }

        // check version
        const GtVersionNumber depVersion = m_plugins.value(name)->version();

        if (depVersion < version)
        {
            gtError().nospace() << QObject::tr("Loading")
                    << meta.moduleId() + QStringLiteral(":");
            gtError()
                    << QObject::tr("Dependency '%1' is outdated!").arg(name)
                    << QObject::tr("(needed: >= %1, current: %2)")
                       .arg(version.toString(), depVersion.toString());
            return false;
        }
        else if (depVersion > version)
        {
            gtInfo().medium()
                    << QObject::tr("Dependency '%1' has a newer version than "
                                   "the module '%2' requires")
                       .arg(name, meta.moduleId())
                    << QObject::tr("(needed: >= %1, current: %2)")
                       .arg(version.toString(), depVersion.toString());
        }
    }

    return true;
}

void
GtModuleLoader::Impl::printDependencies(const ModuleMetaData& meta)
{
    gtWarning() << QString("#### %1 (%2)")
                   .arg(meta.moduleId(), meta.location());

    for (const auto& dep : meta.directDependencies())
    {
        gtWarning() << QObject::tr("####   - %1 (%2)")
            .arg(dep.first, dep.second.toString());
    }
}

bool
GtModuleLoader::Impl::isSuppressed(const ModuleMetaData& meta) const
{
    const auto& moduleId = meta.moduleId();
    const auto& allowedSupprs = meta.suppressorModules();

    // check if there is a set of suppressors for the given moduleId
    const auto it = m_suppressedPlugins.find(moduleId);
    if (it ==  m_suppressedPlugins.end())
        return false;

    // search for intersection between set of suppressors and allowedSupprs
    const auto& supressorList = *it;
    auto res = std::find_if(allowedSupprs.begin(), allowedSupprs.end(),
                [supressorList](const QString& allowedSuppr){
                    return supressorList.contains(allowedSuppr);});

    if (res != allowedSupprs.end())
    {
        gtWarning() << QObject::tr("'%1' is suppressed by '%2'")
                       .arg(moduleId, *res);
        return true;
    }

    return false;
}

/**
 * Loads the meta data from the module json file
 */
void
ModuleMetaData::readFromJson(const QJsonObject &pluginMetaData)
{
    auto json = pluginMetaData.value(QStringLiteral("MetaData")).toObject();

    // read plugin/module id
    m_id = pluginMetaData.value("IID").toString();

    // read dependencies
    QVariantList deps = metaArray(json, QStringLiteral("dependencies"));

    m_deps.clear();
    for (const auto& d : qAsConst(deps))
    {
        QVariantMap mitem = d.toMap();

        auto name = mitem.value(QStringLiteral("name")).toString();
        GtVersionNumber version(mitem.value(QStringLiteral("version")).toString());
        m_deps.push_back(std::make_pair(name, version));
    }

    // get sys_env_vars list
    QVariantList sys_vars = metaArray(json,
                                      QStringLiteral("sys_env_vars"));

    m_envVars.clear();
    for (const QVariant& var : qAsConst(sys_vars))
    {
        QVariantMap mitem = var.toMap();

        const QString name =
            mitem.value(QStringLiteral("name")).toString();
        const QString initVar =
            mitem.value(QStringLiteral("init")).toString();

        if (!m_envVars.contains(name))
        {
            m_envVars.insert(name, initVar);
        }
    }

    // get suppressor list
    QVariantList supprs = metaArray(json,
                                    QStringLiteral("allowSuppressionBy"));

    m_suppression.clear();
    for (const auto & s : qAsConst(supprs))
    {
        m_suppression.push_back(s.toString());
    }
}
