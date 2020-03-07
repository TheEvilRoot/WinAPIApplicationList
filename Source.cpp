#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

class wexception : std::exception {
public:
	std::string message;

	wexception(std::string msg) : message(std::move(msg)) { }
	wexception(const char *msg): message(msg) { }
};

class Application {
private:
	std::string mAppName;
	std::string mPublisher;
	std::string mVersion;
	std::string mInstallDate;
	std::string mLocation;

	std::string parseDate(const std::string& date) {
		if (date.length() < 8) return date;
		std::string ret = date.substr(0, 4) + "/";
		ret += date.substr(4, 2) + "/";
		ret += date.substr(6, 2);
		return ret;
	}

public:
	Application(const std::string& appName,
		const std::string& publisher,
		const std::string& version,
		const std::string& installDate,
		const std::string& location):
		mAppName(appName),
		mPublisher(publisher),
		mVersion(version),
		mInstallDate(parseDate(installDate)),
		mLocation(location) { }

	friend std::ostream& operator << (std::ostream& stream, Application& application) {
		stream << application.mAppName << " " << application.mVersion << "\n\t";
		stream << "Publisher: " << application.mPublisher << "\n\t";
		stream << "Installed " << application.mInstallDate << "\n\t";
		stream << "Into " << application.mLocation << std::endl;

		return stream;
	}
};

class ApplicationBuilder {
private:
	std::string mAppName;
	bool mAppNameSet = false;
	std::string mPublisher;
	bool mPublisherSet = false;
	std::string mVersion;
	bool mVersionSet = false;
	std::string mInstallDate;
	bool mInstallDateSet = false;
	std::string mLocation;
	bool mLocationSet = false;

public:
	ApplicationBuilder() { }

	ApplicationBuilder* setName(const std::string &appName) {
		this->mAppName = appName;
		this->mAppNameSet = true;
		return this;
	}

	ApplicationBuilder* setPublisher(const std::string &appPublisher) {
		this->mPublisher = appPublisher;
		this->mPublisherSet = true;
		return this;
	}

	ApplicationBuilder* setVersion(const std::string &appVersion) {
		this->mVersion = appVersion;
		this->mVersionSet = true;
		return this;
	}
	
	ApplicationBuilder* setInstallDate(const std::string &appInstallDate) {
		this->mInstallDate = appInstallDate;
		this->mInstallDateSet = true;
		return this;
	}

	ApplicationBuilder* setLocation(const std::string& appLocation) {
		this->mLocation = appLocation;
		this->mLocationSet = true;
		return this;
	}

	bool isApplicationReady() {
		return 
			this->mAppNameSet &&
			this->mInstallDateSet &&
			this->mPublisherSet &&
			this->mVersionSet &&
			this->mLocationSet;
	}

	Application* build() {
		if (!isApplicationReady())
			return nullptr;
		return new Application(mAppName, mPublisher, mVersion, mInstallDate, mLocation);
	}

	void reset() {
		this->mAppNameSet = false;
		this->mInstallDateSet = false;
		this->mPublisherSet = false;
		this->mVersionSet = false;
		this->mLocationSet = false;
	}

	bool tryToApplyValue(CHAR* cValueName, DWORD type, BYTE* data) {
		std::string valueName(cValueName);

		if (type == REG_SZ || type == REG_EXPAND_SZ) {

			if (valueName == "DisplayName") {
				std::string displayName((char*)data);
				setName(displayName);
				return true;
			}

			if (valueName == "DisplayVersion") {
				std::string displayVersion((char*)data);
				setVersion(displayVersion);
				return true;
			}

			if (valueName == "InstallDate") {
				std::string installDate((char*)data);
				setInstallDate(installDate);
				return true;
			}

			if (valueName == "InstallLocation") {
				std::string installLocation((char*)data);
				setLocation(installLocation);
				return true;
			}

			if (valueName == "Publisher") {
				std::string publisher((char*)data);
				setPublisher(publisher);
				return true;
			}
		}

		return false;
	}

};

HKEY openKey(HKEY parent, const CHAR *subKey, DWORD access) {
	HKEY key;
	if (RegOpenKeyExA(parent, subKey, 0, access, &key) == ERROR_SUCCESS)
		return key;
	else throw wexception(std::string("Failed to open register entry ") + subKey);
}

HKEY openApplications() {
	return openKey(HKEY_LOCAL_MACHINE, 
		"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\",
		KEY_ENUMERATE_SUB_KEYS);
}

void buildApplication(HKEY key, ApplicationBuilder *builder) {
	size_t index = 0;
	CHAR valueName[255] = { 0 };
	DWORD valueNameSize = 255;
	
	BYTE data[1024];
	DWORD dataSize = 1024;

	DWORD type = 0;

	for (DWORD readError = ERROR_SUCCESS;
		readError == ERROR_SUCCESS; 
		readError = RegEnumValueA(key, 
			index,
			valueName,
			&valueNameSize,
			NULL, &type,
			data, &dataSize), index++, valueNameSize = 255, dataSize = 1024) {
		builder->tryToApplyValue(valueName, type, data);
	}
}

std::vector<Application *> queryApplications(HKEY key) {
	CHAR keyName[255] = { 0 };
	DWORD keySize = 255;
	size_t index = 0;

	std::vector<Application*> applications;

	ApplicationBuilder* builder = new ApplicationBuilder;
	for (DWORD regError = ERROR_SUCCESS;
		regError == ERROR_SUCCESS;
		regError = RegEnumKeyExA(key, index, keyName, &keySize, NULL, NULL, NULL, NULL), index++, keySize = 255) {
		try{
			HKEY subKey = openKey(key, keyName, KEY_QUERY_VALUE);
			buildApplication(subKey, builder);

			if (builder->isApplicationReady()) {
				Application* application = builder->build();
				applications.push_back(application);
			}

			builder->reset();
			RegCloseKey(subKey);
		}
		catch (wexception & we) { }
	}

	return applications;
}

int main() {
	HKEY key = openApplications();
	std::vector<Application *> applications = queryApplications(key);
	RegCloseKey(key);

	std::cout << "Found " << applications.size() << " applications: \n";
	std::cout << "Any key to show the list" << std::endl;
	getchar();
	
	for (const auto& application : applications) {
		system("cls");
		std::cout << "Any key to next, q to exit\n" << std::endl;

		std::cout << *application;
		if (getchar() == 'y') break;
	}

	std::cout << "The end!" << std::endl;

	return 0;
}
