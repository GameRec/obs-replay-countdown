#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>
#include "plugin-support.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#endif

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

static inline QString trObs(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

enum class CountdownMode {
	None = 0,
	AutoChain = 1,
	ActionWindow = 2
};

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

		modeLabel = new QLabel(trObs("OverlayTest"), this);
		modeLabel->setAlignment(Qt::AlignCenter);

		timeLabel = new QLabel("02:30", this);
		timeLabel->setAlignment(Qt::AlignCenter);

		layout->addWidget(modeLabel);
		layout->addWidget(timeLabel);

		applyVisualStyle("#00E676", 100);
		applyCaptureExclusion();
		hide();
	}

	void applyCaptureExclusion()
	{
#ifdef _WIN32
		HWND hwnd = reinterpret_cast<HWND>(winId());
		if (hwnd) {
			SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
		}
#endif
	}

	void updateDisplay(int remainingSec, CountdownMode mode, int scalePercent, int screenIndex, int cornerIndex)
	{
		int mins = std::max(0, remainingSec) / 60;
		int secs = std::max(0, remainingSec) % 60;
		timeLabel->setText(QString("%1:%2")
					   .arg(mins, 2, 10, QChar('0'))
					   .arg(secs, 2, 10, QChar('0')));

		if (mode == CountdownMode::AutoChain) {
			modeLabel->setText(trObs("OverlayAutoSave"));
		} else if (mode == CountdownMode::ActionWindow) {
			modeLabel->setText(trObs("OverlayBufferWindow"));
		} else {
			modeLabel->setText(trObs("OverlayTest"));
		}

		QString accentColor = "#00E676";
		if (remainingSec <= 5) {
			accentColor = "#FF1744";
		} else if (remainingSec <= 20) {
			accentColor = "#FFEA00";
		}

		applyVisualStyle(accentColor, scalePercent);
		adjustSize();
		repositionToScreenAndCorner(screenIndex, cornerIndex);
		applyCaptureExclusion();
	}

protected:
	void showEvent(QShowEvent *event) override
	{
		QWidget::showEvent(event);
		applyCaptureExclusion();
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

	void repositionToScreenAndCorner(int screenIndex, int cornerIndex)
	{
		const auto screens = QGuiApplication::screens();
		QScreen *screen = nullptr;

		if (screenIndex >= 0 && screenIndex < screens.size()) {
			screen = screens.at(screenIndex);
		} else {
			screen = QGuiApplication::primaryScreen();
		}

		if (!screen)
			return;

		QRect avail = screen->availableGeometry();
		const int margin = 28;
		int x = avail.left() + margin;
		int y = avail.top() + margin;

		switch (cornerIndex) {
		case 1:
			x = avail.right() - width() - margin;
			y = avail.top() + margin;
			break;
		case 2:
			x = avail.left() + margin;
			y = avail.bottom() - height() - margin;
			break;
		case 3:
			x = avail.right() - width() - margin;
			y = avail.bottom() - height() - margin;
			break;
		case 0:
		default:
			x = avail.left() + margin;
			y = avail.top() + margin;
			break;
		}
		move(x, y);
	}
};

class ReplayCountdownController : public QWidget {
public:
	ReplayCountdownController(QWidget *parent = nullptr) : QWidget(parent)
	{
		overlay = new CountdownOverlay();
		timer = new QTimer(this);
		timer->setInterval(1000);
		connect(timer, &QTimer::timeout, this, &ReplayCountdownController::onTick);

		buildDockUi();
		populateScreens();
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
			statusLabel->setText(trObs("StatusBufferOff"));
			return;
		}

		obs_frontend_replay_buffer_save();

		int rbTotal = get_obs_replay_buffer_seconds();
		int offset = offsetSpin->value();
		int target = std::max(5, rbTotal - offset);

		activeMode = CountdownMode::AutoChain;
		remainingSeconds = target;
		startOverlayTimer();
		statusLabel->setText(trObs("StatusAutoChainActive").arg(target));
	}

	void triggerActionWindow()
	{
		if (!obs_frontend_replay_buffer_active()) {
			statusLabel->setText(trObs("StatusBufferOff"));
			return;
		}

		int rbTotal = get_obs_replay_buffer_seconds();
		activeMode = CountdownMode::ActionWindow;
		remainingSeconds = rbTotal;
		startOverlayTimer();
		statusLabel->setText(trObs("StatusActionWindow").arg(rbTotal));
	}

	void stopCountdown(const QString &reason)
	{
		timer->stop();
		activeMode = CountdownMode::None;
		remainingSeconds = 0;
		overlay->hide();
		statusLabel->setText(reason);
	}

	void onReplaySavedExternally()
	{
		if (activeMode == CountdownMode::ActionWindow) {
			stopCountdown(trObs("StatusReplaySaved"));
		}
	}

	void refreshBufferInfo()
	{
		int sec = get_obs_replay_buffer_seconds();
		bool active = obs_frontend_replay_buffer_active();
		bufferInfoLabel->setText(QString("%1: %2s (%3)")
						 .arg(trObs("ObsBuffer"))
						 .arg(sec)
						 .arg(active ? trObs("StatusActive") : trObs("StatusStopped")));
	}

private:
	CountdownOverlay *overlay = nullptr;
	QTimer *timer = nullptr;
	CountdownMode activeMode = CountdownMode::None;
	int remainingSeconds = 0;

	QLabel *bufferInfoLabel = nullptr;
	QLabel *statusLabel = nullptr;
	QSpinBox *offsetSpin = nullptr;
	QComboBox *screenCombo = nullptr;
	QComboBox *cornerCombo = nullptr;
	QSpinBox *scaleSpin = nullptr;

	void buildDockUi()
	{
		auto *root = new QVBoxLayout(this);
		root->setContentsMargins(10, 10, 10, 10);
		root->setSpacing(8);

		bufferInfoLabel = new QLabel(QString("%1: --s").arg(trObs("ObsBuffer")), this);
		bufferInfoLabel->setStyleSheet("font-weight: bold;");
		root->addWidget(bufferInfoLabel);

		auto *offsetRow = new QHBoxLayout();
		offsetRow->addWidget(new QLabel(trObs("OffsetLabel"), this));
		offsetSpin = new QSpinBox(this);
		offsetSpin->setRange(0, 60);
		offsetSpin->setValue(5);
		offsetRow->addWidget(offsetSpin);
		root->addLayout(offsetRow);

		auto *screenRow = new QHBoxLayout();
		screenRow->addWidget(new QLabel(trObs("MonitorLabel"), this));
		screenCombo = new QComboBox(this);
		screenRow->addWidget(screenCombo);
		root->addLayout(screenRow);

		auto *cornerRow = new QHBoxLayout();
		cornerRow->addWidget(new QLabel(trObs("PositionLabel"), this));
		cornerCombo = new QComboBox(this);
		cornerCombo->addItems({trObs("PosTopLeft"), trObs("PosTopRight"),
				       trObs("PosBottomLeft"), trObs("PosBottomRight")});
		cornerCombo->setCurrentIndex(1);
		cornerRow->addWidget(cornerCombo);
		root->addLayout(cornerRow);

		auto *scaleRow = new QHBoxLayout();
		scaleRow->addWidget(new QLabel(trObs("ScaleLabel"), this));
		scaleSpin = new QSpinBox(this);
		scaleSpin->setRange(60, 200);
		scaleSpin->setSingleStep(10);
		scaleSpin->setValue(100);
		scaleRow->addWidget(scaleSpin);
		root->addLayout(scaleRow);

		auto *btnRow = new QHBoxLayout();
		auto *testBtn = new QPushButton(trObs("BtnTest"), this);
		auto *stopBtn = new QPushButton(trObs("BtnStop"), this);
		btnRow->addWidget(testBtn);
		btnRow->addWidget(stopBtn);
		root->addLayout(btnRow);

		statusLabel = new QLabel(trObs("StatusIdle"), this);
		statusLabel->setWordWrap(true);
		root->addWidget(statusLabel);

		root->addStretch();

		connect(testBtn, &QPushButton::clicked, this, [this]() {
			activeMode = CountdownMode::None;
			remainingSeconds = 10;
			startOverlayTimer();
			statusLabel->setText(trObs("StatusTesting"));
		});

		connect(stopBtn, &QPushButton::clicked, this, [this]() {
			stopCountdown(trObs("StatusStoppedManual"));
		});

		connect(offsetSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { saveSettings(); });

		connect(screenCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
			saveSettings();
			refreshLiveOverlay();
		});

		connect(cornerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
			saveSettings();
			refreshLiveOverlay();
		});

		connect(scaleSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
			saveSettings();
			refreshLiveOverlay();
		});
	}

	void populateScreens()
	{
		screenCombo->blockSignals(true);
		screenCombo->clear();
		const auto screens = QGuiApplication::screens();
		for (int i = 0; i < screens.size(); ++i) {
			QScreen *s = screens.at(i);
			QRect g = s->geometry();
			QString label = QString("Monitor %1: %2 (%3x%4)")
						.arg(i + 1)
						.arg(s->name())
						.arg(g.width())
						.arg(g.height());
			screenCombo->addItem(label);
		}
		if (screenCombo->count() == 0) {
			screenCombo->addItem("Monitor 1");
		}
		screenCombo->blockSignals(false);
	}

	void refreshLiveOverlay()
	{
		if (overlay && overlay->isVisible()) {
			overlay->updateDisplay(remainingSeconds, activeMode, scaleSpin->value(),
					       screenCombo->currentIndex(), cornerCombo->currentIndex());
		}
	}

	void startOverlayTimer()
	{
		overlay->updateDisplay(remainingSeconds, activeMode, scaleSpin->value(),
				       screenCombo->currentIndex(), cornerCombo->currentIndex());
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
					stopCountdown(trObs("StatusSecondSaved"));
				} else {
					stopCountdown(trObs("StatusBufferStoppedMid"));
				}
			} else {
				stopCountdown(trObs("StatusFinished"));
			}
			return;
		}

		overlay->updateDisplay(remainingSeconds, activeMode, scaleSpin->value(),
				       screenCombo->currentIndex(), cornerCombo->currentIndex());
	}

	void saveSettings()
	{
		config_t *appCfg = obs_frontend_get_app_config();
		if (!appCfg)
			return;
		config_set_int(appCfg, "ReplayCountdown", "OffsetSec", offsetSpin->value());
		config_set_int(appCfg, "ReplayCountdown", "ScreenIndex", screenCombo->currentIndex());
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
		if (config_has_user_value(appCfg, "ReplayCountdown", "ScreenIndex")) {
			int idx = (int)config_get_int(appCfg, "ReplayCountdown", "ScreenIndex");
			if (idx >= 0 && idx < screenCombo->count())
				screenCombo->setCurrentIndex(idx);
		}
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
				g_controller->stopCountdown(trObs("StatusBufferStoppedMid"));
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
		obs_frontend_add_dock_by_id("obs_replay_countdown_dock", obs_module_text("DockTitle"), g_controller);
	}

	g_hotkey_autochain = obs_hotkey_register_frontend(
		"replay_countdown.autochain",
		obs_module_text("HotkeyAutoChain"),
		hotkey_autochain_cb, nullptr);

	g_hotkey_window = obs_hotkey_register_frontend(
		"replay_countdown.window",
		obs_module_text("HotkeyWindow"),
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