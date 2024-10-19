/**
 *
 * @file   bfsConfigLayer.cpp
 * @brief  This is the class implementation for the config interface
 *         layer for the bfs file system.  This is really the static methods
 *         and data for the static class.
 *
 */

/* Include files  */

/* Project include files */
#include <bfsConfigLayer.h>
#include <bfs_log.h>

/* Macros */

/* Globals  */

//
// Class Data

// Create the static class data
unsigned long bfsConfigLayer::bfsConfigLogLevel = (unsigned long)0;
unsigned long bfsConfigLayer::bfsVerboseConfigLogLevel = (unsigned long)0;
bfsCfgStore *bfsConfigLayer::systemConfig = NULL;
string bfsConfigLayer::systemBaseDir;

// Static initializer, make sure this is idenpendent of other layers
bool bfsConfigLayer::bfsConfigLayerInitialized = false;

//
// Class Functions

/*
 * @brief Load a config configuration
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */
#ifndef __BFS_ENCLAVE_MODE
int bfsConfigLayer::loadSystemConfiguration() {

	// Local variables
	char *base;

	// Check to see if this is already loaded
	if (systemConfig != NULL) {
		logMessage(LOG_ERROR_LEVEL,
				   "Trying to load system configuration when already loaded. ");
		// return( -1 );
		return 0;
	}

	// Load the configuration
	try {

		// Get the base filesystem directory, get the system configuration
		if ((base = getenv(BFS_BASEDIR_ENVVAR)) == NULL) {
			logMessage(LOG_ERROR_LEVEL,
					   "Unable to get base environment varable [%s], will "
					   "be unable to find system config.",
					   BFS_BASEDIR_ENVVAR);
			return (-1);
		}
		systemBaseDir = base;
		systemConfig = new bfsCfgStore();
		systemConfig->loadConfigurationFile(systemBaseDir +
											BFS_DEFAULT_SYSCONFIG);

	} catch (bfsCfgParserError *e) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed loading system config, config parse error : %s\n",
				   e->getMessage().c_str());
		delete systemConfig;
		systemConfig = NULL;
		return (-1);
	} catch (bfsCfgError *e) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed loading system config, config error : %s\n",
				   e->getMessage().c_str());
		delete systemConfig;
		systemConfig = NULL;
		return (-1);
	}

	// Return succcesfully
	return (0);
}
#endif

/*
 * @brief Get a configuration item from the config tag
 *
 * @param cfgtag - the item tag to return
 * @return int : item or NULL if not found
 */

bfsCfgItem *bfsConfigLayer::getConfigItem(string cfgtag) {

	// Redirect the query to the config store
	bfsCfgItem *item = NULL;
	try {
		item = systemConfig->queryConfig(cfgtag);
	} catch (bfsCfgError *e) {
		logMessage(LOG_ERROR_LEVEL,
				   "Config Error getting BFS system configuration: %s : %s\n",
				   cfgtag.c_str(), e->getMessage().c_str());
		return (NULL);
	}

	// Return the item
	return (item);
}

/*
 * @brief Get a configuration value from the config tag(returns 0 if found),
 *        and places the value in the "val" ref
 *
 * @param cfgtag - the item tag to return
 * @param val - the place to put the value
 * @return int : item or NULL if not found
 */

int bfsConfigLayer::getConfigItemValue(string cfgtag, string &val) {

	// Get the item, sanity check the type
	bfsCfgItem *item = getConfigItem(cfgtag);
	if (item == NULL) {
		return (-1);
	} else if (item->bfsCfgItemType() != bfsCfgItem_VALUE) {
		logMessage(LOG_ERROR_LEVEL,
				   "Attempting to get single value from compount config :%s",
				   cfgtag.c_str());
		return (-1);
	}

	// Return the item, or NULL if not found
	val = item->bfsCfgItemValue();
	return (0);
}

/*
 * @brief Initialize the config layer state
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsConfigLayer::bfsConfigLayerInit(void) {
	// Create the class log level (assign default)
	if (bfsConfigLogLevel == (unsigned long)0) {
		bfsConfigLogLevel = registerLogLevel("CONFIG_LOG_LEVEL", 1);
	}

	// Create the class (verbose) log level (assign default)
	if (bfsVerboseConfigLogLevel == (unsigned long)0) {
		bfsVerboseConfigLogLevel = registerLogLevel("CONFIG_VRBLOG_LEVEL", 0);
	}

#ifdef __BFS_ENCLAVE_MODE
	// Check to make sure we were able to load the configuration
	int ret = 0;
	if ((ocall_load_system_config(&ret) != SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed to load system configuration, aborting.\n");
		return ret;
	}
#else
	// Check to make sure we were able to load the configuration
	if (bfsConfigLayer::loadSystemConfiguration() != 0) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed to load system configuration, aborting.\n");
		return (-1);
	}
#endif

#ifdef __BFS_ENCLAVE_MODE
	// TODO: QBURKE - you need to add the level setting here.
#else
	bfsCfgItem *config = bfsConfigLayer::getConfigItem(BFS_CFGLYR_CONFIG);
	if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to find config configuration in system config : %s",
				   BFS_CFGLYR_CONFIG);
		return (-1);
	}

	// Normal log level
	if (config->getSubItemByName("log_enabled")->bfsCfgItemValue() == "true") {
		enableLogLevels(bfsConfigLogLevel);
	} else {
		disableLogLevels(bfsConfigLogLevel);
	}

	// Verbose log level
	if (config->getSubItemByName("log_verbose")->bfsCfgItemValue() == "true") {
		enableLogLevels(bfsVerboseConfigLogLevel);
	} else {
		disableLogLevels(bfsVerboseConfigLogLevel);
	}
#endif

	// Log the config layer being initialized, return successfully
	bfsConfigLayerInitialized = true;
	logMessage(CONFIG_LOG_LEVEL, "bfsConfigLayer initialized. ");
	return (0);
}

/*
 * @brief test the configuration layer implementation
 *
 * @param none
 * @return int : 0 if
 */
#ifndef __BFS_ENCLAVE_MODE
int bfsConfigLayer::bfsConfigLayerUtest(void) {

	// Local variables
	bfsCfgStore uStore;
	bfsCfgItem *item;
	int idx;

	// The sample file storage values
	const char *sampleConfigs[] = {"config1",
								   "value1",
								   "config2[0]",
								   "value2",
								   "config2[1]",
								   "value3",
								   "config3.config4",
								   "value4",
								   "config3.config5[0]",
								   "value5",
								   "config3.config5[1]",
								   "value6",
								   "config3.config5[2]",
								   "value7",
								   "config3.config6.config7",
								   "value8",
								   "config9.config10",
								   "value9",
								   "config11[0].config12",
								   "value10",
								   "config11[1].config13",
								   "value11",
								   "config11[2].config14.config15",
								   "value12",
								   "config11[2].config14.config16",
								   "value13",
								   "config11[2].config14.config20[0]",
								   "value14",
								   "config11[2].config14.config20[1]",
								   "value15",
								   "config11[2].config14.config17.config18",
								   "value16",
								   "config11[2].config14.config17.config19",
								   "value17",
								   NULL,
								   NULL};

	try {

		// Load the configuration
		bfsConfigLayer::loadSystemConfiguration();

		// TODO: fix the above call because it will not load the right thing
		// Just make all of these calls to use a bfsConfigStore instead of the
		// system configuration

		uStore.loadConfigurationFile(bfsConfigLayer::getSystemBaseDirectory() +
									 "/config/sample.cfg");

		// Query the values of the configuration
		idx = 0;
		while (sampleConfigs[idx * 2] != NULL) {
			if ((item = uStore.queryConfig(sampleConfigs[idx * 2])) == NULL) {
				fprintf(stderr, "Unable to find config, aborting : %s\n",
						sampleConfigs[idx * 2]);
				return (-1);
			}
			if (item->bfsCfgItemValue() != sampleConfigs[(idx * 2) + 1]) {
				fprintf(stderr,
						"Incorrect config, aborting : \"%s\" != \"%s\"\n",
						item->bfsCfgItemValue().c_str(),
						sampleConfigs[(idx * 2) + 1]);
				return (-1);
			}

			// Log the success, move to next element
			printf("Config found correctly : %s -> %s\n",
				   sampleConfigs[idx * 2], item->bfsCfgItemValue().c_str());
			idx++;
		}

	} catch (bfsCfgParserError *e) {
		fprintf(stderr, "Parse Error : %s\n", e->getMessage().c_str());
		return (-1);
	} catch (bfsCfgError *e) {
		fprintf(stderr, "Config Error : %s\n", e->getMessage().c_str());
		return (-1);
	}

	// Log, return succesfully
	printf("All configurations correctly found.\n");
	return (0);
}
#endif