// WWE 2K14 launcher page. Talks to MainForm.cs through WebView2 messages; opened in a plain browser (no host), it
// shows sample data so the layout can be checked.
const host = window.chrome?.webview;

const resolutionHints = {
    720: 'Native Xbox 360 resolution (1280×720). Lightest on the GPU.',
    1080: 'Rendered at 2560×1440, shown at 1920×1080: sharper than native 1080p.',
    1440: 'Rendered at 2560×1440, twice the console resolution.',
    2160: 'Rendered at 2560×1440 and scaled up to 3840×2160.',
};

const elements = {
    gameDot: document.getElementById('game-status-dot'),
    gamePath: document.getElementById('game-folder-path'),
    gameStatus: document.getElementById('game-folder-status'),
    resolutionButtons: [...document.querySelectorAll('[data-resolution]')],
    vsync: document.getElementById('vsync'),
    windowModeButtons: [...document.querySelectorAll('[data-window-mode]')],
    keep60: document.getElementById('keep60'),
    shadows: document.getElementById('shadows'),
    fpsCounter: document.getElementById('fps-counter'),
    anisoButtons: [...document.querySelectorAll('[data-aniso]')],
    aaButtons: [...document.querySelectorAll('[data-aa]')],
    upscalerButtons: [...document.querySelectorAll('[data-upscaler]')],
    dlcList: document.getElementById('dlc-list'),
    dlcCount: document.getElementById('dlc-count'),
    dlcProgress: document.getElementById('dlc-progress'),
    dlcProgressFile: document.getElementById('dlc-progress-file'),
    dlcProgressCount: document.getElementById('dlc-progress-count'),
    dlcProgressFill: document.getElementById('dlc-progress-fill'),
    dlcButtons: ['add-dlc-files', 'add-dlc-folder'].map(id => document.getElementById(id)),
    modsList: document.getElementById('mods-list'),
    modsCount: document.getElementById('mods-count'),
    modProgress: document.getElementById('mod-progress'),
    modProgressFile: document.getElementById('mod-progress-file'),
    modProgressCount: document.getElementById('mod-progress-count'),
    modProgressFill: document.getElementById('mod-progress-fill'),
    modButtons: ['add-mod-files', 'add-mod-folder', 'open-mods-folder'].map(id => document.getElementById(id)),
    play: document.getElementById('play'),
    playLabel: document.getElementById('play-label'),
    tabs: [...document.querySelectorAll('[data-tab]')],
    achList: document.getElementById('ach-list'),
    achCount: document.getElementById('ach-count'),
    achScore: document.getElementById('ach-score'),
    achScoreTotal: document.getElementById('ach-score-total'),
    achProgressFill: document.getElementById('ach-progress-fill'),
};

// ---------- DLC / Achievements tabs ----------

function showTab(name) {
    elements.tabs.forEach(tab => {
        const selected = tab.dataset.tab === name;
        tab.setAttribute('aria-checked', String(selected));   // reuses the selected-segment style
        tab.setAttribute('aria-selected', String(selected));
        document.getElementById(tab.getAttribute('aria-controls')).hidden = !selected;
    });
}

elements.tabs.forEach(tab => tab.addEventListener('click', () => showTab(tab.dataset.tab)));
// The launcher always opens on Achievements.
showTab('achievements');

// Icons extracted by the game from its own achievement data, served from userdata\achievements (MainForm.cs).
const lockIcon = '<svg viewBox="0 0 24 24" aria-hidden="true"><rect x="5.5" y="10.5" width="13" height="9.5" rx="2"/><path d="M8.5 10.5V8a3.5 3.5 0 0 1 7 0v2.5"/></svg>';
let achievementData = [];
let achievementFilter = 'all';

function achievementIcon(a, className) {
    const icon = document.createElement('div');
    icon.className = className;
    if (a.image && host) {
        const image = document.createElement('img');
        image.src = `https://data.wwe2k14/${a.image}`;
        image.alt = '';
        image.addEventListener('error', () => image.remove(), { once: true });
        icon.appendChild(image);
    }
    if (!a.unlocked) {
        const lock = document.createElement('span');
        lock.className = 'ach-lock';
        lock.innerHTML = lockIcon;
        icon.appendChild(lock);
    }
    return icon;
}

function sortedAchievements(list) {
    // Unlocked first (most recent on top), then locked in the game's order.
    return list.filter(a => a.unlocked).sort((a, b) => b.unlockedAt - a.unlockedAt)
        .concat(list.filter(a => !a.unlocked));
}

function renderAchievementWindow() {
    const list = achievementData;
    const unlocked = list.filter(a => a.unlocked);
    const percent = list.length ? Math.round(unlocked.length * 100 / list.length) : 0;
    document.getElementById('ach-modal-count').textContent = `${unlocked.length} / ${list.length}`;
    document.getElementById('ach-modal-percent').textContent = `${percent}%`;
    document.getElementById('ach-modal-fill').style.width = `${percent}%`;
    document.querySelectorAll('[data-filter]').forEach(button => {
        button.setAttribute('aria-checked', String(button.dataset.filter === achievementFilter));
    });

    const grid = document.getElementById('ach-grid');
    grid.replaceChildren();
    const shown = sortedAchievements(list).filter(a =>
        achievementFilter === 'all' || (achievementFilter === 'unlocked') === Boolean(a.unlocked));
    if (!shown.length) {
        const empty = document.createElement('p');
        empty.className = 'dlc-empty';
        empty.textContent = !list.length ? 'Start the game once to load the achievement list.'
            : achievementFilter === 'unlocked' ? 'No achievements unlocked yet.' : 'Every achievement is unlocked!';
        grid.appendChild(empty);
        return;
    }
    shown.forEach(a => {
        const card = document.createElement('article');
        card.className = a.unlocked ? 'ach-card is-unlocked' : 'ach-card is-locked';

        const copy = document.createElement('div');
        copy.className = 'ach-card-copy';
        const top = document.createElement('div');
        top.className = 'ach-card-top';
        const name = document.createElement('h3');
        name.className = 'ach-card-name';
        name.textContent = a.name || `Achievement ${a.id}`;
        top.append(name);
        const desc = document.createElement('p');
        desc.className = 'ach-card-desc';
        desc.textContent = a.description;
        const state = document.createElement('span');
        state.className = 'ach-card-state';
        state.textContent = a.unlocked
            ? `Unlocked ${new Date(a.unlockedAt).toLocaleDateString(undefined, { year: 'numeric', month: 'short', day: 'numeric' })}`
            : 'Locked';
        copy.append(top, desc, state);

        card.append(achievementIcon(a, 'ach-card-icon'), copy);
        grid.appendChild(card);
    });
}

const achievementsModal = document.getElementById('achievements-modal');
let modalReturnFocus = null;

function openAchievementWindow() {
    modalReturnFocus = document.activeElement;
    renderAchievementWindow();
    achievementsModal.classList.add('is-visible');
    document.getElementById('ach-grid').scrollTop = 0;
    document.getElementById('ach-modal-close').focus();
}

function closeAchievementWindow() {
    achievementsModal.classList.remove('is-visible');
    modalReturnFocus?.focus?.();
}

document.getElementById('open-achievements').addEventListener('click', openAchievementWindow);
document.getElementById('ach-modal-close').addEventListener('click', closeAchievementWindow);
achievementsModal.addEventListener('click', event => {
    if (event.target === achievementsModal) closeAchievementWindow();
});
// Controls window: controller diagram with a legend, and the keyboard keys for the same buttons.
const controlsModal = document.getElementById('controls-modal');

function showControls(view) {
    document.querySelectorAll('[data-controls]').forEach(button => {
        button.setAttribute('aria-checked', String(button.dataset.controls === view));
    });
    document.getElementById('controls-controller').hidden = view !== 'controller';
    document.getElementById('controls-keyboard').hidden = view !== 'keyboard';
}

function openControlsWindow() {
    modalReturnFocus = document.activeElement;
    controlsModal.classList.add('is-visible');
    document.getElementById('controls-modal-close').focus();
}

function closeControlsWindow() {
    controlsModal.classList.remove('is-visible');
    modalReturnFocus?.focus?.();
}

document.querySelectorAll('[data-controls]').forEach(button => {
    button.addEventListener('click', () => showControls(button.dataset.controls));
});
showControls('controller');
document.getElementById('open-controls').addEventListener('click', openControlsWindow);
document.getElementById('controls-modal-close').addEventListener('click', closeControlsWindow);
controlsModal.addEventListener('click', event => {
    if (event.target === controlsModal) closeControlsWindow();
});

document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && achievementsModal.classList.contains('is-visible')) closeAchievementWindow();
    if (event.key === 'Escape' && controlsModal.classList.contains('is-visible')) closeControlsWindow();
});
document.querySelectorAll('[data-filter]').forEach(button => {
    button.addEventListener('click', () => {
        achievementFilter = button.dataset.filter;
        renderAchievementWindow();
    });
});

function renderAchievements(data) {
    elements.achList.replaceChildren();
    const list = data?.achievements || [];
    achievementData = list;
    if (achievementsModal.classList.contains('is-visible')) renderAchievementWindow();
    if (!list.length) {
        elements.achCount.textContent = '';
        elements.achScore.textContent = '0%';
        elements.achScoreTotal.textContent = '';
        elements.achProgressFill.style.width = '0%';
        const empty = document.createElement('li');
        empty.className = 'dlc-empty';
        empty.textContent = 'Start the game once to load the achievement list.';
        elements.achList.appendChild(empty);
        return;
    }
    const unlocked = list.filter(a => a.unlocked);
    const percent = Math.round(unlocked.length * 100 / list.length);
    elements.achCount.textContent = `${unlocked.length} / ${list.length}`;
    elements.achScore.textContent = `${percent}%`;
    elements.achScoreTotal.textContent = `${unlocked.length} of ${list.length} unlocked`;
    elements.achProgressFill.style.width = `${percent}%`;

    sortedAchievements(list).forEach(a => {
        const row = document.createElement('li');
        row.className = a.unlocked ? 'ach-item is-unlocked' : 'ach-item is-locked';
        row.title = 'Open the achievements window';
        row.addEventListener('click', openAchievementWindow);
        row.appendChild(achievementIcon(a, 'ach-item-icon'));
        const copy = document.createElement('div');
        copy.className = 'ach-item-copy';
        const name = document.createElement('span');
        name.className = 'ach-item-name';
        name.textContent = a.name || `Achievement ${a.id}`;
        const desc = document.createElement('span');
        desc.className = 'ach-item-desc';
        desc.textContent = a.unlocked
            ? `${a.description}${a.description ? ' · ' : ''}Unlocked ${new Date(a.unlockedAt).toLocaleDateString()}`
            : a.description;
        copy.append(name, desc);
        row.append(copy);
        elements.achList.appendChild(row);
    });
}

let settings = {
    resolution: '1080',
    vsync: true,
    windowMode: 'borderless',
    fullscreen: false,
    keep60: true,
    shadows: true,
    fpsCounter: true,
    anisotropic: '4x',
    antiAliasing: 'fxaa',
    upscaler: 'off',
    useTitleUpdate: false,
    gameFolder: '',
    previousGameFolder: ''
};
let state = { game: { ok: false, message: '' }, exe: { ok: false }, running: false, dlc: [], mods: [], tu: { installed: false, built: false, building: false, name: '' } };

function send(type, extra = {}) {
    if (host) {
        host.postMessage({ type, ...extra });
    }
}

function saveSettings() {
    send('save', { settings });
}

// ---------- Rendering ----------

function renderSettings() {
    elements.resolutionButtons.forEach(button => {
        button.setAttribute('aria-checked', String(button.dataset.resolution === settings.resolution));
        button.tabIndex = button.dataset.resolution === settings.resolution ? 0 : -1;
    });
    if (elements.resolutionHint) {
        elements.resolutionHint.textContent = resolutionHints[settings.resolution] || '';
    }
    if (elements.vsync) elements.vsync.checked = settings.vsync;
    if (elements.keep60) elements.keep60.checked = settings.keep60;
    if (elements.shadows) elements.shadows.checked = settings.shadows !== false;
    if (elements.fpsCounter) elements.fpsCounter.checked = Boolean(settings.fpsCounter);

    const currentWindowMode = (settings.windowMode || (settings.fullscreen ? 'fullscreen' : 'borderless')).toLowerCase();
    elements.windowModeButtons.forEach(button => {
        const on = button.dataset.windowMode.toLowerCase() === currentWindowMode;
        button.setAttribute('aria-checked', String(on));
        button.tabIndex = on ? 0 : -1;
    });

    const currentAniso = (settings.anisotropic || '4x').toLowerCase();
    elements.anisoButtons.forEach(button => {
        const on = button.dataset.aniso.toLowerCase() === currentAniso;
        button.setAttribute('aria-checked', String(on));
        button.tabIndex = on ? 0 : -1;
    });

    const currentAa = (settings.antiAliasing || 'fxaa').toLowerCase();
    elements.aaButtons.forEach(button => {
        const btnVal = button.dataset.aa.toLowerCase();
        const on = btnVal === currentAa || (btnVal === 'max' && currentAa === 'extreme');
        button.setAttribute('aria-checked', String(on));
        button.tabIndex = on ? 0 : -1;
    });

    const currentUpscaler = (settings.upscaler || 'off').toLowerCase();
    elements.upscalerButtons.forEach(button => {
        const on = button.dataset.upscaler.toLowerCase() === currentUpscaler;
        button.setAttribute('aria-checked', String(on));
        button.tabIndex = on ? 0 : -1;
    });

    renderTitleUpdate();
}

// Optional title update: Vanilla always works; "Updated" once a TU file is installed and its exe is built.
function renderTitleUpdate() {
    const tu = state.tu || {};
    const ready = tu.installed && tu.built;
    const useTu = settings.useTitleUpdate && ready;
    document.querySelectorAll('#version-group .segment').forEach(button => {
        const on = (button.dataset.version === 'tu') === useTu;
        button.setAttribute('aria-checked', String(on));
        button.tabIndex = on ? 0 : -1;
        if (button.dataset.version === 'tu') button.disabled = !ready;
    });
    const label = document.getElementById('tu-action-label');
    const action = document.getElementById('tu-action');
    action.disabled = state.running || tu.building || Boolean(state.tc?.busy);
    label.textContent = tu.building ? 'Building...' : !tu.installed ? 'Install update' : !tu.built ? 'Build' : 'Remove Update';
    action.title = tu.building
        ? 'Building the updated game in the console window. This takes a while; the launcher updates when it is done.'
        : !tu.installed ? 'Playing the original disc version. Have the title update file (tu00000002_00000000)? Install it here.'
        : !tu.built ? `${tu.name} installed. Build it once to play the updated game.`
        : useTu ? `Playing with ${tu.name}.` : `${tu.name} ready. Pick "Updated" to play with it.`;
    document.querySelector('#version-group [data-version="tu"]').title = ready ? tu.name : 'Install a title update first';
}

function renderGame() {
    elements.gamePath.textContent = settings.gameFolder || 'No folder chosen';
    elements.gamePath.title = settings.gameFolder;
    elements.gameStatus.textContent = state.game.message;
    elements.gameStatus.classList.toggle('is-error', !state.game.ok);
    elements.gameDot.classList.toggle('is-ok', state.game.ok);
    renderPlay();
}

function renderTotalConversion() {
    const busy = Boolean(state.running || state.tc?.busy);
    const tcButtons = [
        document.getElementById('btn-install-tc'),
        document.getElementById('btn-install-tc-modtab'),
        document.getElementById('btn-restore-vanilla')
    ];
    tcButtons.forEach(btn => {
        if (btn) btn.disabled = busy;
    });
    const restoreBtn = document.getElementById('btn-restore-vanilla');
    if (restoreBtn) {
        restoreBtn.style.display = 'inline-flex';
    }
}

function renderPlay() {
    elements.play.classList.toggle('is-running', state.running);
    elements.playLabel.textContent = state.running ? 'Running' : 'Play';
    elements.play.disabled = state.running || !state.game.ok || !state.exe.ok;
    elements.play.title = !state.exe.ok ? 'wwe2k14.exe was not found. Build the game first.'
        : !state.game.ok ? state.game.message : '';
    elements.dlcButtons.forEach(button => {
        if (button) button.disabled = state.running;
    });
    elements.modButtons.forEach(button => {
        if (button) button.disabled = state.running;
    });
}

function formatSize(bytes) {
    if (bytes >= 1024 ** 3) return `${(bytes / 1024 ** 3).toFixed(1)} GB`;
    if (bytes >= 1024 ** 2) return `${(bytes / 1024 ** 2).toFixed(1)} MB`;
    return `${Math.max(1, Math.round(bytes / 1024))} KB`;
}

function renderDlc() {
    elements.dlcList.replaceChildren();
    elements.dlcCount.textContent = state.dlc.length ? String(state.dlc.length) : '';
    if (!state.dlc.length) {
        const empty = document.createElement('li');
        empty.className = 'dlc-empty';
        empty.textContent = 'No DLC installed yet.';
        elements.dlcList.appendChild(empty);
        return;
    }
    state.dlc.forEach(item => {
        const row = document.createElement('li');
        row.className = 'dlc-item';

        const copy = document.createElement('div');
        copy.className = 'dlc-item-copy';
        const name = document.createElement('span');
        name.className = 'dlc-item-name';
        name.textContent = item.displayName;
        const meta = document.createElement('span');
        meta.className = 'dlc-item-meta';
        meta.textContent = `${item.name} · ${formatSize(item.sizeBytes)}`;
        copy.append(name, meta);

        const remove = document.createElement('button');
        remove.type = 'button';
        remove.className = 'action-pill is-danger';
        remove.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 7h14M10 11v6M14 11v6M6.5 7l.8 11.2a1.5 1.5 0 0 0 1.5 1.3h6.4a1.5 1.5 0 0 0 1.5-1.3L17.5 7M9.5 7V5.5a1 1 0 0 1 1-1h3a1 1 0 0 1 1 1V7"/></svg><span>Remove</span>';
        remove.setAttribute('aria-label', `Remove ${item.displayName}`);
        remove.disabled = state.running;
        remove.addEventListener('click', () => {
            if (window.confirm(`Remove "${item.displayName}"? You can add the package again later.`)) {
                send('removeDlc', { name: item.name });
            }
        });

        row.append(copy, remove);
        elements.dlcList.appendChild(row);
    });
}

function renderMods(mods) {
    elements.modsList.replaceChildren();
    const list = mods || [];
    elements.modsCount.textContent = list.length ? String(list.length) : '';
    if (!list.length) {
        const empty = document.createElement('li');
        empty.className = 'dlc-empty';
        empty.textContent = 'No mods installed yet. Add a package (.zip) or folder to get started.';
        elements.modsList.appendChild(empty);
        return;
    }
    list.forEach(mod => {
        const row = document.createElement('li');
        row.className = `dlc-item mod-item${mod.enabled ? '' : ' is-disabled'}`;

        const toggleLabel = document.createElement('label');
        toggleLabel.className = 'toggle mod-toggle';
        toggleLabel.title = mod.enabled ? 'Enabled (click to disable)' : 'Disabled (click to enable)';
        const input = document.createElement('input');
        input.type = 'checkbox';
        input.checked = Boolean(mod.enabled);
        input.disabled = state.running;
        input.addEventListener('change', () => {
            row.classList.toggle('is-disabled', !input.checked);
            toggleLabel.title = input.checked ? 'Enabled (click to disable)' : 'Disabled (click to enable)';
            send('toggleMod', { id: mod.id, enabled: input.checked });
        });
        const track = document.createElement('span');
        track.className = 'toggle-track';
        track.setAttribute('aria-hidden', 'true');
        toggleLabel.append(input, track);

        const copy = document.createElement('div');
        copy.className = 'dlc-item-copy';
        const name = document.createElement('span');
        name.className = 'dlc-item-name';
        name.textContent = mod.name;
        const meta = document.createElement('span');
        meta.className = 'dlc-item-meta';
        const details = [
            mod.fileCount ? `${mod.fileCount} file${mod.fileCount === 1 ? '' : 's'}` : '',
            formatSize(mod.sizeBytes),
            mod.summary
        ].filter(Boolean).join(' · ');
        meta.textContent = details;
        copy.append(name, meta);

        const remove = document.createElement('button');
        remove.type = 'button';
        remove.className = 'action-pill is-danger';
        remove.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 7h14M10 11v6M14 11v6M6.5 7l.8 11.2a1.5 1.5 0 0 0 1.5 1.3h6.4a1.5 1.5 0 0 0 1.5-1.3L17.5 7M9.5 7V5.5a1 1 0 0 1 1-1h3a1 1 0 0 1 1 1V7"/></svg><span>Remove</span>';
        remove.setAttribute('aria-label', `Remove ${mod.name}`);
        remove.disabled = state.running;
        remove.addEventListener('click', () => {
            if (window.confirm(`Remove mod "${mod.name}"?`)) {
                send('removeMod', { id: mod.id });
            }
        });

        row.append(toggleLabel, copy, remove);
        elements.modsList.appendChild(row);
    });
}

function showToast(message, isError = false) {
    const toast = document.createElement('p');
    toast.className = isError ? 'toast is-error' : 'toast';
    toast.setAttribute('role', isError ? 'alert' : 'status');
    toast.textContent = message;
    document.querySelectorAll('.toast').forEach(old => old.remove());
    document.body.appendChild(toast);
    window.setTimeout(() => toast.remove(), isError ? 5200 : 2600);
}

// ---------- Controls ----------

elements.resolutionButtons.forEach(button => {
    button.addEventListener('click', () => {
        settings.resolution = button.dataset.resolution;
        renderSettings();
        saveSettings();
    });
});

// Arrow keys move between the resolution choices, like a radio group.
document.getElementById('resolution-group').addEventListener('keydown', event => {
    const step = { ArrowRight: 1, ArrowDown: 1, ArrowLeft: -1, ArrowUp: -1 }[event.key];
    if (!step) return;
    event.preventDefault();
    const index = elements.resolutionButtons.findIndex(b => b.dataset.resolution === settings.resolution);
    const next = elements.resolutionButtons[(index + step + elements.resolutionButtons.length) % elements.resolutionButtons.length];
    next.click();
    next.focus();
});

[['vsync', elements.vsync], ['keep60', elements.keep60], ['shadows', elements.shadows], ['fpsCounter', elements.fpsCounter]].forEach(([key, input]) => {
    if (input) {
        input.addEventListener('change', () => {
            settings[key] = input.checked;
            saveSettings();
        });
    }
});

elements.windowModeButtons.forEach(button => {
    button.addEventListener('click', () => {
        settings.windowMode = button.dataset.windowMode;
        settings.fullscreen = (settings.windowMode === 'fullscreen');
        renderSettings();
        saveSettings();
    });
});

document.getElementById('window-mode-group')?.addEventListener('keydown', event => {
    const step = { ArrowRight: 1, ArrowDown: 1, ArrowLeft: -1, ArrowUp: -1 }[event.key];
    if (!step) return;
    event.preventDefault();
    const currentMode = (settings.windowMode || (settings.fullscreen ? 'fullscreen' : 'borderless')).toLowerCase();
    const index = elements.windowModeButtons.findIndex(b => b.dataset.windowMode.toLowerCase() === currentMode);
    const next = elements.windowModeButtons[(index + step + elements.windowModeButtons.length) % elements.windowModeButtons.length];
    next.click();
    next.focus();
});

elements.anisoButtons.forEach(button => {
    button.addEventListener('click', () => {
        settings.anisotropic = button.dataset.aniso;
        renderSettings();
        saveSettings();
    });
});

elements.aaButtons.forEach(button => {
    button.addEventListener('click', () => {
        settings.antiAliasing = button.dataset.aa;
        renderSettings();
        saveSettings();
    });
});

elements.upscalerButtons.forEach(button => {
    button.addEventListener('click', () => {
        settings.upscaler = button.dataset.upscaler;
        renderSettings();
        saveSettings();
    });
});

document.getElementById('browse-game').addEventListener('click', () => send('browseGame'));
document.getElementById('browse-game-image').addEventListener('click', () => send('browseGameImage'));
document.querySelectorAll('#version-group .segment').forEach(button => {
    button.addEventListener('click', () => {
        settings.useTitleUpdate = button.dataset.version === 'tu';
        saveSettings();
        renderTitleUpdate();
    });
});
document.getElementById('tu-action').addEventListener('click', () => {
    const tu = state.tu || {};
    send(!tu.installed ? 'installTu' : !tu.built ? 'buildTu' : 'removeTu');
});
document.getElementById('add-dlc-files').addEventListener('click', () => send('addDlcFiles'));
document.getElementById('add-dlc-folder').addEventListener('click', () => send('addDlcFolder'));
document.getElementById('add-music').addEventListener('click', () => send('addMusic'));
document.getElementById('add-logo').addEventListener('click', () => send('addLogo'));
document.getElementById('import-ps3-logos').addEventListener('click', () => send('importPs3Logos'));

// Custom logos: the game's Paint Tool save and the logo library (LogoStore.cs).
// state.logos = { available, message, items: [{ slot, preset, image }], library: [{ id, inGame, image }] }.
let logoView = 'game';
const libraryModal = document.getElementById('library-modal');

function openLibraryInfo() {
    modalReturnFocus = document.activeElement;
    libraryModal.classList.add('is-visible');
    document.getElementById('library-modal-ok').focus();
}

function closeLibraryInfo() {
    libraryModal.classList.remove('is-visible');
    modalReturnFocus?.focus?.();
}

document.getElementById('logo-library-toggle').addEventListener('click', () => {
    logoView = logoView === 'library' ? 'game' : 'library';
    if (logoView === 'library') openLibraryInfo();
    renderLogos(state.logos);
});
document.getElementById('library-modal-close').addEventListener('click', closeLibraryInfo);
document.getElementById('library-modal-ok').addEventListener('click', closeLibraryInfo);
libraryModal.addEventListener('click', event => {
    if (event.target === libraryModal) closeLibraryInfo();
});
document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && libraryModal.classList.contains('is-visible')) closeLibraryInfo();
});

function iconButton(className, label, path, onClick) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = className;
    button.disabled = state.running;
    button.setAttribute('aria-label', label);
    button.title = label;
    button.innerHTML = `<svg viewBox="0 0 24 24" aria-hidden="true"><path d="${path}"/></svg>`;
    button.addEventListener('click', event => {
        event.stopPropagation();
        onClick();
    });
    return button;
}

function emptyNote(list, text) {
    const empty = document.createElement('li');
    empty.className = 'dlc-empty';
    empty.textContent = text;
    list.appendChild(empty);
}

function renderLogos(logos) {
    const list = document.getElementById('logo-list');
    const libraryList = document.getElementById('logo-library-list');
    list.replaceChildren();
    libraryList.replaceChildren();
    const items = (logos && logos.items) || [];
    const library = (logos && logos.library) || [];
    const inLibrary = logoView === 'library';
    const toggle = document.getElementById('logo-library-toggle');
    toggle.classList.toggle('is-accent', inLibrary);
    toggle.setAttribute('aria-pressed', String(inLibrary));
    list.hidden = inLibrary;
    libraryList.hidden = !inLibrary;
    document.getElementById('logos-count').textContent = inLibrary
        ? `${library.length} in library · ${items.length} / 20 in game`
        : (items.length ? `${items.length} / 20` : '');
    document.getElementById('logos-hint').textContent = inLibrary
        ? 'Click a logo to put it in the game or take it out (check mark = in the game).'
        : 'PNG, JPG, BMP or GIF, fitted to 256x256 with transparency. Up to 20 in the game, as many as you like in the Library.';
    document.getElementById('add-logo').disabled = state.running;
    document.getElementById('import-ps3-logos').disabled = state.running;

    if (inLibrary) {
        if (!library.length) {
            emptyNote(libraryList, 'Your library is empty. Add logos or import a PS3 save.');
            return;
        }
        library.forEach((logo, index) => {
            const tile = document.createElement('li');
            tile.className = 'logo-tile is-pickable' + (logo.inGame ? ' is-in-game' : '');
            tile.tabIndex = 0;
            tile.setAttribute('role', 'button');
            tile.setAttribute('aria-pressed', String(logo.inGame));
            tile.title = logo.inGame ? 'In the game: click to take it out' : 'Click to put it in the game';
            const img = document.createElement('img');
            img.src = logo.image;
            img.alt = `Library logo ${index + 1}`;
            tile.appendChild(img);
            if (logo.inGame) {
                const check = document.createElement('span');
                check.className = 'logo-check';
                check.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 12.5l4.5 4.5L19 7.5"/></svg>';
                tile.appendChild(check);
            }
            tile.appendChild(iconButton('logo-remove', 'Delete from the library',
                'M5 7h14M10 7V5h4v2M7 7l1 12h8l1-12', () => send('deleteLibraryLogo', { id: logo.id })));
            const pick = () => { if (!state.running) send('toggleLibraryLogo', { id: logo.id }); };
            tile.addEventListener('click', pick);
            tile.addEventListener('keydown', event => {
                if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); pick(); }
            });
            libraryList.appendChild(tile);
        });
        return;
    }

    if (!logos || !logos.available) {
        emptyNote(list, (logos && logos.message) || 'No logo save yet. In the game, save a logo once in the Paint Tool.');
        return;
    }
    items.forEach(logo => {
        const tile = document.createElement('li');
        tile.className = 'logo-tile';
        const img = document.createElement('img');
        img.src = logo.image;
        img.alt = logo.preset ? 'Default logo' : `Custom logo ${logo.slot + 1}`;
        tile.appendChild(img);
        if (logo.preset) {
            const tag = document.createElement('span');
            tag.className = 'logo-tag';
            tag.textContent = 'Default';
            tile.appendChild(tag);
        } else {
            tile.appendChild(iconButton('logo-remove', `Take custom logo ${logo.slot + 1} out of the game (it stays in the Library)`,
                'M6 6l12 12M18 6 6 18', () => send('removeLogo', { slot: logo.slot })));
        }
        list.appendChild(tile);
    });
}

function renderMusic(songs) {
    const list = document.getElementById('music-list');
    list.replaceChildren();
    document.getElementById('music-count').textContent = songs.length ? String(songs.length) : '';
    if (!songs.length) {
        const empty = document.createElement('li');
        empty.className = 'dlc-empty';
        empty.textContent = 'No songs yet. Add some to use them in game.';
        list.appendChild(empty);
        return;
    }
    songs.forEach(song => {
        const row = document.createElement('li');
        row.className = 'dlc-item';
        const copy = document.createElement('div');
        copy.className = 'dlc-item-copy';
        const name = document.createElement('span');
        name.className = 'dlc-item-name';
        name.textContent = song.title;
        const meta = document.createElement('span');
        meta.className = 'dlc-item-meta';
        meta.textContent = `${song.name.split('.').pop().toUpperCase()} · ${formatSize(song.sizeBytes)}`;
        copy.append(name, meta);
        const remove = document.createElement('button');
        remove.type = 'button';
        remove.className = 'action-pill is-danger';
        remove.innerHTML = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 7h14M10 11v6M14 11v6M6.5 7l.8 11.2a1.5 1.5 0 0 0 1.5 1.3h6.4a1.5 1.5 0 0 0 1.5-1.3L17.5 7M9.5 7V5.5a1 1 0 0 1 1-1h3a1 1 0 0 1 1 1V7"/></svg><span>Remove</span>';
        remove.setAttribute('aria-label', `Remove ${song.title}`);
        remove.addEventListener('click', () => send('removeMusic', { name: song.name }));
        row.append(copy, remove);
        list.appendChild(row);
    });
}
document.getElementById('open-dlc-folder').addEventListener('click', () => send('openFolder', { which: 'dlc' }));
document.getElementById('btn-install-tc')?.addEventListener('click', () => send('installTotalConversion'));
document.getElementById('btn-install-tc-modtab')?.addEventListener('click', () => send('installTotalConversion'));
document.getElementById('btn-restore-vanilla')?.addEventListener('click', () => send('restoreVanilla'));
document.getElementById('add-mod-files').addEventListener('click', () => send('addModFiles'));
document.getElementById('add-mod-folder').addEventListener('click', () => send('addModFolder'));
document.getElementById('open-mods-folder').addEventListener('click', () => send('openFolder', { which: 'mods' }));
document.querySelectorAll('[data-folder]').forEach(button => {
    button.addEventListener('click', () => send('openFolder', { which: button.dataset.folder }));
});
elements.play.addEventListener('click', () => send('play', { settings }));
// Social links open in the default browser (the launcher itself never navigates away).
document.querySelectorAll('.social-link').forEach(link => {
    link.addEventListener('click', () => send('openUrl', { url: link.dataset.url }));
});

// ---------- Messages from the launcher ----------

function handleMessage(message) {
    switch (message.type) {
        case 'state':
            settings = message.settings;
            state = message;
            renderSettings();
            renderGame();
            renderDlc();
            renderMods(message.mods || []);
            renderAchievements(message.achievements);
            renderMusic(message.music || []);
            renderLogos(message.logos);
            renderTotalConversion();
            break;
        case 'gameRunning':
            state.running = message.running;
            renderPlay();
            renderDlc();
            renderMods(state.mods || []);
            renderTotalConversion();
            break;
        case 'tcProgress': {
            const tcBar = document.getElementById('tc-progress');
            if (tcBar) {
                tcBar.hidden = false;
                document.getElementById('tc-progress-step').textContent = message.step || '';
                document.getElementById('tc-progress-count').textContent = message.total > 0 ? `${message.percent}%` : '';
                document.getElementById('tc-progress-fill').style.width = `${message.percent}%`;
            }
            break;
        }
        case 'tcProgressDone': {
            const tcBarDone = document.getElementById('tc-progress');
            if (tcBarDone) tcBarDone.hidden = true;
            break;
        }
        case 'modProgress':
            elements.modProgress.hidden = false;
            elements.modProgressFile.textContent = `Installing ${message.file}`;
            elements.modProgressCount.textContent = message.count > 1
                ? `${message.index + 1} / ${message.count} · ${message.percent}%`
                : `${message.percent}%`;
            elements.modProgressFill.style.width = `${message.percent}%`;
            break;
        case 'modProgressDone':
            elements.modProgress.hidden = true;
            break;
        case 'dlcProgress':
            elements.dlcProgress.hidden = false;
            elements.dlcProgressFile.textContent = `Installing ${message.file}`;
            elements.dlcProgressCount.textContent = message.count > 1
                ? `${message.index + 1} / ${message.count} · ${message.percent}%`
                : `${message.percent}%`;
            elements.dlcProgressFill.style.width = `${message.percent}%`;
            break;
        case 'dlcResult': {
            elements.dlcProgress.hidden = true;
            const failed = message.results.filter(r => !r.ok);
            const installed = message.results.filter(r => r.ok && r.message === 'Installed.').length;
            if (failed.length) {
                const first = failed[0];
                showToast(`${first.displayName || first.file}: ${first.message}${failed.length > 1 ? ` (+${failed.length - 1} more)` : ''}`, true);
            } else if (installed) {
                showToast(installed === 1 ? `${message.results.find(r => r.ok).displayName || 'DLC'} installed.` : `${installed} DLC packages installed.`);
            } else {
                showToast('Already installed.');
            }
            break;
        }
        case 'toast':
            showToast(message.message, message.error);
            break;
    }
}

if (host) {
    host.addEventListener('message', event => handleMessage(event.data));
    send('init');
} else {
    handleMessage({
        type: 'state',
        settings: { resolution: '1440', vsync: true, windowMode: 'borderless', fullscreen: false, keep60: true, gameFolder: 'C:\\Games\\WWE 2K14 (USA, Europe)' },
        game: { ok: true, message: 'Game files ready' },
        exe: { ok: true },
        running: false,
        dlc: [
            { name: 'sample_pack_1', displayName: 'Sample Pack 1', sizeBytes: 412_000_000 },
            { name: 'sample_pack_2', displayName: 'Sample Pack 2', sizeBytes: 96_000_000 },
        ],
        mods: [
            { id: 'sample_attire', name: 'Retro Attire Pack', enabled: true, relativePath: 'Retro Attire Pack', sizeBytes: 15_400_000, fileCount: 4, summary: 'Superstars (4)' },
            { id: 'sample_arena', name: 'Raw 2000 Arena', enabled: false, relativePath: 'Raw 2000 Arena', sizeBytes: 32_100_000, fileCount: 2, summary: 'Arenas (2)' },
        ],
        achievements: {
            titleId: '545408B2',
            achievements: [
                { id: 1, name: 'Sample Achievement One', description: 'Win a sample match.', unlocked: true, unlockedAt: Date.now() - 3_600_000 },
                { id: 2, name: 'Sample Achievement Two', description: 'Finish a sample mode.', unlocked: false, unlockedAt: 0 },
                { id: 3, name: 'Sample Achievement Three', description: 'Do something sample.', unlocked: false, unlockedAt: 0 },
            ],
        },
    });
}
