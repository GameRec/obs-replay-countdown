#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>
#include "plugin-support.h"
#include <QComboBox>
#include <QDockWidget>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

enum class CountdownMode {
	None = 0,
	AutoChain = 1,
	ActionWindow = 2
};

// Odczyt aktualnej dlugosci bufora powtorki (w sekundach) z ustawien profilu OBS
static int get_obs_replay_buffer_seconds()
{
	config_t *cfg = obs_frontend_get_profile_config();
	if (!cfg)
		return 150;

	const char *mode = config_get_string(cfg, "Output", "Mode");
	int seconds = 0;

	if (mode && QString::compare(QString::fromUtf8(mode), "Advanced", Qt::CaseInsensitive) == 0) {
		seconds = (int)config_get_int(cfg, "AdvOut", "RecRBTime");
	} else {
		seconds = (int)config_get_int(cfg, "SimpleOutput", "RecRBTime");
	}

	return (seconds > 0) ? seconds : 150;
}

// Przezroczyste okno nakladki (click-through) wyswietlajace stoper na ekranie
class CountdownOverlay : public QWidget {
public:
	CountdownOverlay() : QWidget(nullptr)
	{
		setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
			       Qt::Tool | Qt::WindowTransparentForInput |
			       Qt::WindowDoesNotAcceptFocus);
		setAttribute(Qt::WA_TranslucentBackground);
		setAttribute(Qt::WA_ShowWithoutActivating);

		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(16, 10, 16, 10);
		layout->setSpacing(2);

		modeLabel = new QLabel("REPLAY BUFFER", this);
		modeLabel->setAlignment(Qt::AlignCenter);

		timeLabel = new QLabel("02:30", this);
		timeLabel->setAlignment(Qt::AlignCenter);

		layout->addWidget(modeLabel);
		layout->addWidget(timeLabel);

		applyVisualStyle("#00E676", 100);
		hide();
	}

	void updateDisplay(int remainingSec, CountdownMode mode, int scalePercent, int cornerIndex)
	{
		int mins = std::max(0, remainingSec) / 60;
		int secs = std::max(0, remainingSec) % 60;
		timeLabel->setText(QString("%1:%2")
					   .arg(mins, 2, 10, QChar('0'))
					   .arg(secs, 2, 10, QChar('0')));

		if (mode == CountdownMode::AutoChain) {
			modeLabel->setText("AUTO-SAVE IN");
		} else if (mode == CountdownMode::ActionWindow) {
			modeLabel->setText("BUFFER WINDOW");
		} else {
			modeLabel->setText("TEST OVERLAY");
		}

		QString accentColor = "#00E676"; // zielony
		if (remainingSec <= 5) {
			accentColor = "#FF1744"; // czerwony
		} else if (remainingSec <= 20) {
			accentColor = "#FFEA00"; // zolty
		}

		applyVisualStyle(accentColor, scalePercent);
		adjustSize();
		repositionToCorner(cornerIndex);
	}

private:
	QLabel *modeLabel = nullptr;
	QLabel *timeLabel = nullptr;

	void applyVisualStyle(const QString &hexColor, int scalePercent)
	{
		double factor = std::clamp(scalePercent, 60, 200) / 100.0;
		int titlePt = std::max(8, (int)(10 * factor));
		int timePt = std::max(14, (int)(26 * factor));

		setStyleSheet(QString(
			"QWidget {"
			"  background-color: rgba(15, 18, 24, 210);"
			"  border: 2px solid %1;"
			"  border-radius: 10px;"
			"}"
			"QLabel {"
			"  background: transparent;"
			"  border: none;"
			"}")
				      .arg(hexColor));

		modeLabel->setStyleSheet(QString("color: #B0BEC5; font-family: 'Segoe UI', sans-serif; "
						 "font-size: %1pt; font-weight: bold; letter-spacing: 1px;")
						 .arg(titlePt));

		timeLabel->setStyleSheet(QString("color: %1; font-family: 'Consolas', 'Segoe UI', monospace; "
						 "font-size: %2pt; font-weight: bold;")
						 .arg(hexColor)
						 .arg(timePt));
	}

	void repositionToCorner(int cornerIndex)
	{
		QScreen *screen = QGuiApplication::primaryScreen();
		if (!screen)
			return;

		QRect avail = screen->availableGeometry();
		const int margin = 28;
		int x = avail.left() + margin;
		int y = avail.top() + margin;

		switch (cornerIndex) {
		case 1: // Top-Right
			x = avail.right() - width() - margin;
			y = avail.top() + margin;
			break;
		case 2: // Bottom-Left
			x = avail.left() + margin;
			y = avail.bottom() - height() - margin;
			break;
		case 3: // Bottom-Right
			x = avail.right() - width() - margin;
			y = avail.bottom() - height() - margin;
			break;
		case 0: // Top-Left
		default:
			x = avail.left() + margin;
			y = avail.top() + margin;
			break;
		}
		move(x, y);
	}
};

// Glowny kontroler wtyczki i dok w OBS
class ReplayCountdownController : public QWidget {
public:
	ReplayCountdownController(QWidget *parent = nullptr) : QWidget(parent)
	{
		overlay = new CountdownOverlay();
		timer = new QTimer(this);
		timer->setInterval(1000);
		connect(timer, &QTimer::timeout, this, &ReplayCountdownController::onTick);

		buildDockUi();
		loadSettings();
		refreshBufferInfo();
	}

	~ReplayCountdownController() override
	{
		saveSettings();
		if (timer)
			timer->stop();
		if (overlay) {
			overlay->hide();
			delete overlay;
			overlay = nullptr;
		}
	}

	void triggerAutoChain()
	{
		if (!obs_frontend_replay_buffer_active()) {
			statusLabel->setText("Status: Replay Buffer is OFF!");
			return;
		}

		// Najpierw zapisz biezaca powtorke
		obs_frontend_replay_buffer_save();

		int rbTotal = get_obs_replay_buffer_seconds();
		int offset = offsetSpin->value();
		int target = std::max(5, rbTotal - offset);

		activeMode = CountdownMode::AutoChain;
		remainingSeconds = target;
		startOverlayTimer();
		statusLabel->setText(QString("Status: Auto-Chain active (%1s)").arg(target));
	}

	void triggerActionWindow()
	{
		if (!obs_frontend_replay_buffer_active()) {
			statusLabel->setText("Status: Replay Buffer is OFF!");
			return;
		}

		int rbTotal = get_obs_replay_buffer_seconds();
		activeMode = CountdownMode::ActionWindow;
		remainingSeconds = rbTotal;
		startOverlayTimer();
		statusLabel->setText(QString("Status: Action Window (%1s)").arg(rbTotal));
	}

	void stopCountdown(const QString &reason = "Status: Idle")
	{
		timer->stop();
		activeMode = CountdownMode::None;
		remainingSeconds = 0;
		overlay->hide();
		statusLabel->setText(reason);
	}

	void onReplaySavedExternally()
	{
		// Jesli uzytkownik kliknal zwykly zapis w trybie ActionWindow, chowamy licznik
		if (activeMode == CountdownMode::ActionWindow) {
			stopCountdown("Status: Replay saved! Timer cleared.");
		}
	}

	void refreshBufferInfo()
	{
		int sec = get_obs_replay_buffer_seconds();
		bool active = obs_frontend_replay_buffer_active();
		bufferInfoLabel->setText(QString("OBS Buffer: %1s (%2)")
						 .arg(sec)
						 .arg(active ? "ACTIVE" : "STOPPED"));
	}

private:
	CountdownOverlay *overlay = nullptr;
	QTimer *timer = nullptr;
	CountdownMode activeMode = CountdownMode::None;
	int remainingSeconds = 0;

	QLabel *bufferInfoLabel = nullptr;
	QLabel *statusLabel = nullptr;
	QSpinBox *offsetSpin = nullptr;
	QComboBox *cornerCombo = nullptr;
	QSpinBox *scaleSpin = nullptr;

	void buildDockUi()
	{
		auto *root = new QVBoxLayout(this);
		root->setContentsMargins(10, 10, 10, 10);
		root->setSpacing(8);

		bufferInfoLabel = new QLabel("OBS Buffer: --s", this);
		bufferInfoLabel->setStyleSheet("font-weight: bold;");
		root->addWidget(bufferInfoLabel);

		// Margines bezpieczenstwa dla Auto-Chain
		auto *offsetRow = new QHBoxLayout();
		offsetRow->addWidget(new QLabel("Auto-Chain safety offset (sec):", this));
		offsetSpin = new QSpinBox(this);
		offsetSpin->setRange(0, 60);
		offsetSpin->setValue(5);
		offsetRow->addWidget(offsetSpin);
		root->addLayout(offsetRow);

		// Pozycja na ekranie
		auto *cornerRow = new QHBoxLayout();
		cornerRow->addWidget(new QLabel("Overlay position:", this));
		cornerCombo = new QComboBox(this);
		cornerCombo->addItems({"Top-Left", "Top-Right", "Bottom-Left", "Bottom-Right"});
		cornerCombo->setCurrentIndex(1); // domyslnie Top-Right
		cornerRow->addWidget(cornerCombo);
		root->addLayout(cornerRow);

		// Skala licznika
		auto *scaleRow = new QHBoxLayout();
		scaleRow->addWidget(new QLabel("Overlay scale (%):", this));
		scaleSpin = new QSpinBox(this);
		scaleSpin->setRange(60, 200);
		scaleSpin->setSingleStep(10);
		scaleSpin->setValue(100);
		scaleRow->addWidget(scaleSpin);
		root->addLayout(scaleRow);

		// Przyciski akcji / testu
		auto *btnRow = new QHBoxLayout();
		auto *testBtn = new QPushButton("Test Overlay (10s)", this);
		auto *stopBtn = new QPushButton("Stop / Hide", this);
		btnRow->addWidget(testBtn);
		btnRow->addWidget(stopBtn);
		root->addLayout(btnRow);

		statusLabel = new QLabel("Status: Idle (configure keys in Settings -> Hotkeys)", this);
		statusLabel->setWordWrap(true);
		root->addWidget(statusLabel);

		root->addStretch();

		connect(testBtn, &QPushButton::clicked, this, [this]() {
			activeMode = CountdownMode::None;
			remainingSeconds = 10;
			startOverlayTimer();
			statusLabel->setText("Status: Testing overlay...");
		});

		connect(stopBtn, &QPushButton::clicked, this, [this]() {
			stopCountdown("Status: Stopped manually.");
		});

		connect(offsetSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { saveSettings(); });
		connect(cornerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
			saveSettings();
			if (overlay->isVisible()) {
				overlay->updateDisplay(remainingSeconds, activeMode, scaleSpin->value(),
						       cornerCombo->currentIndex());
			}
		});
		connect(scaleSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
			saveSettings();
			if (overlay->isVisible()) {
				overlay->updateDisplay(remainingSeconds, activeMode, scaleSpin->value(),
						       cornerCombo->currentIndex());
			}
		});
	}

	void startOverlayTimer()
	{
		overlay->updateDisplay(remainingSeconds, activeMode, scaleSpin->value(), cornerCombo->currentIndex());
		overlay->show();
		timer->start();
	}

	void onTick()
	{
		remainingSeconds--;

		if (remainingSeconds <= 0) {
			if (activeMode == CountdownMode::AutoChain) {
				if (obs_frontend_replay_buffer_active()) {
					obs_frontend_replay_buffer_save();
					stopCountdown("Status: Second replay auto-saved!");
				} else {
					stopCountdown("Status: Buffer stopped before auto-save.");
				}
			} else {
				stopCountdown("Status: Countdown finished.");
			}
			return;
		}

		overlay->updateDisplay(remainingSeconds, activeMode, scaleSpin->value(), cornerCombo->currentIndex());
	}

	void saveSettings()
	{
		config_t *appCfg = obs_frontend_get_app_config();
		if (!appCfg)
			return;
		config_set_int(appCfg, "ReplayCountdown", "OffsetSec", offsetSpin->value());
		config_set_int(appCfg, "ReplayCountdown", "CornerIndex", cornerCombo->currentIndex());
		config_set_int(appCfg, "ReplayCountdown", "ScalePercent", scaleSpin->value());
		config_save_safe(appCfg, "tmp", nullptr);
	}

	void loadSettings()
	{
		config_t *appCfg = obs_frontend_get_app_config();
		if (!appCfg)
			return;
		if (config_has_user_value(appCfg, "ReplayCountdown", "OffsetSec"))
			offsetSpin->setValue((int)config_get_int(appCfg, "ReplayCountdown", "OffsetSec"));
		if (config_has_user_value(appCfg, "ReplayCountdown", "CornerIndex"))
			cornerCombo->setCurrentIndex((int)config_get_int(appCfg, "ReplayCountdown", "CornerIndex"));
		if (config_has_user_value(appCfg, "ReplayCountdown", "ScalePercent"))
			scaleSpin->setValue((int)config_get_int(appCfg, "ReplayCountdown", "ScalePercent"));
	}
};

static ReplayCountdownController *g_controller = nullptr;
static obs_hotkey_id g_hotkey_autochain = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id g_hotkey_window = OBS_INVALID_HOTKEY_ID;

static void hotkey_autochain_cb(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (!pressed || !g_controller)
		return;

	QMetaObject::invokeMethod(g_controller, []() {
		if (g_controller)
			g_controller->triggerAutoChain();
	}, Qt::QueuedConnection);
}

static void hotkey_window_cb(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (!pressed || !g_controller)
		return;

	QMetaObject::invokeMethod(g_controller, []() {
		if (g_controller)
			g_controller->triggerActionWindow();
	}, Qt::QueuedConnection);
}

static void on_frontend_event(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);
	if (!g_controller)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		QMetaObject::invokeMethod(g_controller, []() {
			if (g_controller)
				g_controller->refreshBufferInfo();
		}, Qt::QueuedConnection);
		break;

	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		QMetaObject::invokeMethod(g_controller, []() {
			if (g_controller) {
				g_controller->refreshBufferInfo();
				g_controller->stopCountdown("Status: Replay Buffer stopped.");
			}
		}, Qt::QueuedConnection);
		break;

	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED:
		QMetaObject::invokeMethod(g_controller, []() {
			if (g_controller)
				g_controller->onReplaySavedExternally();
		}, Qt::QueuedConnection);
		break;

	default:
		break;
	}
}

static void save_load_hotkeys(obs_data_t *save_data, bool saving, void *private_data)
{
	UNUSED_PARAMETER(private_data);
	if (saving) {
		obs_data_array_t *arr1 = obs_hotkey_save(g_hotkey_autochain);
		obs_data_set_array(save_data, "replay_countdown_autochain_hotkey", arr1);
		obs_data_array_release(arr1);

		obs_data_array_t *arr2 = obs_hotkey_save(g_hotkey_window);
		obs_data_set_array(save_data, "replay_countdown_window_hotkey", arr2);
		obs_data_array_release(arr2);
	} else {
		obs_data_array_t *arr1 = obs_data_get_array(save_data, "replay_countdown_autochain_hotkey");
		obs_hotkey_load(g_hotkey_autochain, arr1);
		obs_data_array_release(arr1);

		obs_data_array_t *arr2 = obs_data_get_array(save_data, "replay_countdown_window_hotkey");
		obs_hotkey_load(g_hotkey_window, arr2);
		obs_data_array_release(arr2);
	}
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "Replay Countdown plugin loaded (version %s)", PLUGIN_VERSION);

	auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (mainWindow) {
		g_controller = new ReplayCountdownController();
		obs_frontend_add_dock_by_id("obs_replay_countdown_dock", "Replay Countdown", g_controller);
	}

	g_hotkey_autochain = obs_hotkey_register_frontend(
		"replay_countdown.autochain",
		"Replay Countdown: Auto-Chain Save (Save + Countdown)",
		hotkey_autochain_cb, nullptr);

	g_hotkey_window = obs_hotkey_register_frontend(
		"replay_countdown.window",
		"Replay Countdown: Action Window (Countdown Only)",
		hotkey_window_cb, nullptr);

	obs_frontend_add_event_callback(on_frontend_event, nullptr);
	obs_frontend_add_save_callback(save_load_hotkeys, nullptr);

	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(on_frontend_event, nullptr);
	obs_frontend_remove_save_callback(save_load_hotkeys, nullptr);

	if (g_hotkey_autochain != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(g_hotkey_autochain);
	if (g_hotkey_window != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(g_hotkey_window);

	g_controller = nullptr;
	obs_log(LOG_INFO, "Replay Countdown plugin unloaded");
}