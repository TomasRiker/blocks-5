#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

/*** Klasse fuer das virtuelle Dateisystem ***/

#include "file.h"

class FileSystem : public Singleton<FileSystem>
{
	friend class Singleton<FileSystem>;

public:
	enum FileMode
	{
		FM_READ,
		FM_WRITE,
		FM_TEST,
		FM_LIST,
		FM_DELETE
	};

	void closeFile(File* p_file);
	std::string evalPath(const std::string& path) const;
	std::string getAppHomeDirectory() const;

	// Der Ordner, in dem das Spiel liegt, absolut und mit Schraegstrich am
	// Ende. Dort stehen die mitgelieferten Levels, Kampagnen und Skins.
	// Absolut, weil main.cpp data.zip als Wurzel einhaengt: ein relativer Pfad
	// landete zur Laufzeit im Archiv statt auf der Platte. Einmal beim Start
	// gemerkt, weil ein Dateidialog unter Windows das Arbeitsverzeichnis
	// verstellen kann.
	const std::string& getGameDirectory() const { return gameDirectory; }

	// Wo eine Datei mit Inhalten wirklich liegt: erst im Spielordner nachsehen,
	// dann im Benutzerverzeichnis. Der Spielordner gewinnt, damit eine alte
	// Kopie im Benutzerverzeichnis die mitgelieferte nicht verdecken kann - was
	// jahrelang genau das Problem war. Gibt es keine von beiden, kommt der Pfad
	// im Benutzerverzeichnis zurueck: dort haette sie hingehoert.
	std::string resolveContentPath(const std::string& relative) const;

	// Gehoert diese Datei dem Spiel? Das ist zugleich die Antwort auf
	// "unloeschbar", "nicht ueberschreibbar" und "unter diesem Namen darf kein
	// Editor speichern" - eine eigene Liste braucht es dafuer nicht mehr.
	bool isShippedContent(const std::string& relative);
	std::string getCurrentDir() const;
	std::string getPathDirectory(const std::string& path) const;
	std::string getPathFilename(const std::string& path) const;
	File* openFile(const std::string& filename, FileMode mode = FM_READ);
	bool fileExists(const std::string& filename);
	bool deleteFile(const std::string& filename);
	bool copyFile(const std::string& source, const std::string& dest);
	bool createDirectory(const std::string& directory);
	bool deleteDirectory(const std::string& directory);
	std::string readStringFromFile(const std::string& filename);
	bool writeStringToFile(const std::string& text, const std::string& filename);
	void popCurrentDir();
	void pushCurrentDir(const std::string& dir);
	void convertPath(const std::string& path, std::string& filePath, std::string& objectName, std::string& password) const;
	std::string evalRelativePath(const std::string& path, const std::string& basePath) const;
	bool isAbsolutePath(const std::string& path) const;
	std::list<std::string> listDirectory(const std::string& directory);

private:
	FileSystem();
	~FileSystem();

	std::stack<std::string> dirStack;
	std::string gameDirectory;
	uint primes[256];
};

#endif