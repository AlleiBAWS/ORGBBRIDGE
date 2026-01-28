#ifndef VANGUARD96BRIDGE_H
#define VANGUARD96BRIDGE_H

#include "OpenRGBPluginInterface.h"
#include "ResourceManagerInterface.h"

#include <QObject>
#include <QString>
#include <QtPlugin>
#include <QWidget>
#include <QTimer>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QMenu>

#include "Vanguard96Controller.h"

class Vanguard96Bridge : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID)
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    Vanguard96Bridge();
    ~Vanguard96Bridge() override;

    // Plugin info
    OpenRGBPluginInfo   GetPluginInfo() override;
    unsigned int        GetPluginAPIVersion() override;

    // Lifecycle
    void      Load(ResourceManagerInterface* resource_manager_ptr) override;
    QWidget*  GetWidget() override;
    QMenu*    GetTrayMenu() override;
    void      Unload() override;

    // ResourceManager pekare (delas av alla plugin-instanser)
    static ResourceManagerInterface* RMPointer;

private slots:
    void onUpdateTimer();
    void onDeviceSelectionChanged(int index);
    void onEnableToggled(bool checked);

private:
    void rebuildDeviceList();
    bool getSelectedDeviceColor(uint8_t& r, uint8_t& g, uint8_t& b);
    void loadSettings();
    void saveSettings();

    QWidget*   mainWidget   = nullptr;
    QComboBox* deviceCombo  = nullptr;
    QCheckBox* enableCheck  = nullptr;
    QLabel*    statusLabel  = nullptr;
    QTimer*    updateTimer  = nullptr;

    Vanguard96Controller vanguard;
    int      mirroredIndex = -1;
    bool     hasLastColor  = false;
    uint8_t  lastR = 0, lastG = 0, lastB = 0;
};

#endif // VANGUARD96BRIDGE_H
