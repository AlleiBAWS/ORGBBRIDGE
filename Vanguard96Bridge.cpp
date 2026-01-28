#include "Vanguard96Bridge.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDebug>
#include <QSettings>


// OBS: du måste se till att RGBController-headern finns i include-path.
// I OpenRGB-källan heter den vanligtvis "RGBController.h" eller ligger i
// "RGBController/RGBController.h". Anpassa include nedan så det matchar
// din kopia av OpenRGB.

#include "RGBController.h"   // ev. "RGBController/RGBController.h"

ResourceManagerInterface* Vanguard96Bridge::RMPointer = nullptr;

// ----------------------------------------------------------
// Plugin info
// ----------------------------------------------------------

OpenRGBPluginInfo Vanguard96Bridge::GetPluginInfo()
{
    printf("[VanguardBridge] Loading plugin info.\n");

    OpenRGBPluginInfo info;
    info.Name         = "Vanguard Bridge 1.2";
    info.Description  = "Mirror an OpenRGB device's color to Corsair Vanguard 96";
    info.Version      = VERSION_STRING;
    info.Commit       = GIT_COMMIT_ID;   // eller "AlleiBAWS"
    info.URL          = "https://github.com/AlleiBAWS/ORGBBRIDGE";

    info.Icon.load(":/Vanguard96Bridge.png");   // återanvänd sample-ikonen

    info.Location     = OPENRGB_PLUGIN_LOCATION_DEVICES;
    info.Label        = "Vanguard Bridge";
    info.TabIconString = "Vanguard Bridge";
    info.TabIcon.load(":/Vanguard96Bridge.png");

    return info;
}

unsigned int Vanguard96Bridge::GetPluginAPIVersion()
{
    printf("[VanguardBridge] Loading plugin API version.\n");
    return OPENRGB_PLUGIN_API_VERSION;
}

// ----------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------

Vanguard96Bridge::Vanguard96Bridge()
{
    printf("[VanguardBridge] Constructor.\n");
}

Vanguard96Bridge::~Vanguard96Bridge()
{
    printf("[VanguardBridge] Destructor.\n");
}

void Vanguard96Bridge::Load(ResourceManagerInterface* resource_manager_ptr)
{
    printf("[VanguardBridge] Load.\n");

    RMPointer = resource_manager_ptr;

    // Skapa UI första gången
    if (!mainWidget)
    {
        mainWidget = new QWidget(nullptr);
        auto* vbox = new QVBoxLayout(mainWidget);

        deviceCombo = new QComboBox(mainWidget);
        enableCheck = new QCheckBox("Enable Vanguard bridge", mainWidget);
        statusLabel = new QLabel("Status: idle", mainWidget);

        vbox->addWidget(new QLabel("Source device:", mainWidget));
        vbox->addWidget(deviceCombo);
        vbox->addWidget(enableCheck);
        vbox->addWidget(statusLabel);
        vbox->addStretch(1);

        connect(deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &Vanguard96Bridge::onDeviceSelectionChanged);
        connect(enableCheck, &QCheckBox::toggled,
                this, &Vanguard96Bridge::onEnableToggled);
    }

        // Bygg listan utan att trigga currentIndexChanged-signaler
    if (deviceCombo)
    {
        deviceCombo->blockSignals(true);
        rebuildDeviceList();
        deviceCombo->blockSignals(false);
    }
    else
    {
        rebuildDeviceList();
    }

    // Läs in tidigare val + enabled-state
    loadSettings();


    // Starta timern som vanligt om du inte redan gör det här
    // updateTimer->start(50);


    if (!updateTimer)
    {
        updateTimer = new QTimer(this);
        connect(updateTimer, &QTimer::timeout,
                this, &Vanguard96Bridge::onUpdateTimer);
        updateTimer->start(50); // ca 20 fps
    }

    if (statusLabel)
        statusLabel->setText("Status: loaded");
}

QWidget* Vanguard96Bridge::GetWidget()
{
    printf("[VanguardBridge] GetWidget.\n");
    return mainWidget;
}

QMenu* Vanguard96Bridge::GetTrayMenu()
{
    printf("[VanguardBridge] GetTrayMenu.\n");
    QMenu* menu = new QMenu("Vanguard Bridge");
    // (kan lägga till snabbkommandon här senare)
    return menu;
}

void Vanguard96Bridge::Unload()
{
    printf("[VanguardBridge] Unload.\n");

    if (updateTimer)
    {
        updateTimer->stop();
        updateTimer->deleteLater();
        updateTimer = nullptr;
    }

    vanguard.close();
}

// ----------------------------------------------------------
// UI helpers
// ----------------------------------------------------------

void Vanguard96Bridge::rebuildDeviceList()
{
    if (!deviceCombo)
        return;

    deviceCombo->clear();
    mirroredIndex = -1;

    if (!RMPointer)
    {
        deviceCombo->addItem("No ResourceManager");
        return;
    }

    // Nytt: hämta vektorn med controllers från ResourceManagerInterface
    auto &controllers = RMPointer->GetRGBControllers();   // ← använder API:t du ser i felet

    if (controllers.empty())
    {
        deviceCombo->addItem("No devices");
        return;
    }

    for (unsigned int i = 0; i < controllers.size(); i++)
    {
        RGBController* ctrl = controllers[i];
        if (!ctrl)
            continue;

        QString name = QString::fromStdString(ctrl->name);
        deviceCombo->addItem(name, QVariant::fromValue<int>(i));
    }
}


void Vanguard96Bridge::onDeviceSelectionChanged(int index)
{
    if (index < 0)
    {
        mirroredIndex = -1;
        if (statusLabel)
            statusLabel->setText("Status: no device selected");
        return;
    }

    QVariant data = deviceCombo->itemData(index);
    if (data.isValid())
        mirroredIndex = data.toInt();
    else
        mirroredIndex = index;

    hasLastColor = false;

    if (statusLabel)
        statusLabel->setText(QString("Status: mirroring device %1").arg(mirroredIndex));
	
	saveSettings();

}

void Vanguard96Bridge::onEnableToggled(bool checked)
{
    hasLastColor = false;

    if (statusLabel)
        statusLabel->setText(checked ? "Status: enabled" : "Status: disabled");
	
	saveSettings();
}

// ----------------------------------------------------------
// Device color fetch
// ----------------------------------------------------------

bool Vanguard96Bridge::getSelectedDeviceColor(uint8_t& r, uint8_t& g, uint8_t& b)
{
    if (!RMPointer)
        return false;
    if (mirroredIndex < 0)
        return false;

    // Hämta controller-vektorn från ResourceManager
    auto &controllers = RMPointer->GetRGBControllers();

    if (mirroredIndex >= static_cast<int>(controllers.size()))
        return false;

    RGBController* ctrl = controllers[static_cast<unsigned int>(mirroredIndex)];
    if (!ctrl)
        return false;

    // De flesta controllers har en färgvektor 'colors' av typen RGBColor (uint32_t)
    if (ctrl->colors.empty())
        return false;

    RGBColor c = ctrl->colors[0];

    // RGBColor är ett heltal, plocka ut komponenter med hjälpfunktionerna (definierade i RGBController.h)
    r = RGBGetRValue(c);
    g = RGBGetGValue(c);
    b = RGBGetBValue(c);

    return true;
}


// ----------------------------------------------------------
// Timer – här händer magin
// ----------------------------------------------------------

void Vanguard96Bridge::onUpdateTimer()
{
    if (!enableCheck || !enableCheck->isChecked())
        return;

    uint8_t r, g, b;
    if (!getSelectedDeviceColor(r, g, b))
        return;

    if (hasLastColor && r == lastR && g == lastG && b == lastB)
        return;

    if (!vanguard.isOpen() && !vanguard.open())
    {
        if (statusLabel)
            statusLabel->setText("Status: failed to open Vanguard");
        return;
    }

    bool ok = vanguard.setGlobalColor(r, g, b, 255);
    if (!ok)
    {
        if (statusLabel)
            statusLabel->setText("Status: write error");
        return;
    }

    lastR = r;
    lastG = g;
    lastB = b;
    hasLastColor = true;
}


void Vanguard96Bridge::loadSettings()
{
    QSettings settings("AlleiBAWS", "Vanguard96Bridge");

    qDebug() << "[VanguardBridge] loadSettings(): file =" << settings.fileName();

    // 1) Alltid starta enabled
    if (enableCheck)
    {
        enableCheck->blockSignals(true);   // undvik att trigga onEnableToggled här
        enableCheck->setChecked(true);
        enableCheck->blockSignals(false);
    }

    if (!deviceCombo)
    {
        qDebug() << "[VanguardBridge] loadSettings(): deviceCombo is null";
        return;
    }

    int count = deviceCombo->count();
    qDebug() << "[VanguardBridge] loadSettings(): deviceCombo count =" << count;

    // Läs senast valda comborad, default = 0 (första raden)
    int storedComboIndex = settings.value("comboIndex", 0).toInt();
    qDebug() << "[VanguardBridge] loadSettings(): storedComboIndex =" << storedComboIndex;

    if (storedComboIndex < 0 || storedComboIndex >= count)
    {
        qDebug() << "[VanguardBridge] loadSettings(): storedComboIndex out of range";
        return;
    }

    qDebug() << "[VanguardBridge] loadSettings(): setting combo index to" << storedComboIndex;

    // Detta triggar onDeviceSelectionChanged() → sätter mirroredIndex korrekt → sparar igen
    deviceCombo->setCurrentIndex(storedComboIndex);
}




void Vanguard96Bridge::saveSettings()
{
    QSettings settings("AlleiBAWS", "Vanguard96Bridge");

    int comboIndex = -1;
    if (deviceCombo)
        comboIndex = deviceCombo->currentIndex();

    qDebug() << "[VanguardBridge] saveSettings(): mirroredIndex =" << mirroredIndex
             << "comboIndex =" << comboIndex
             << "settings file =" << settings.fileName();

    settings.setValue("comboIndex", comboIndex);
}



