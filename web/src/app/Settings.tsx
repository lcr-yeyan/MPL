import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useState,
  type ReactNode,
} from 'react';
import { isTauri } from '../lib/api';

export type ThemeMode = 'light' | 'dark' | 'system';

export interface Settings {
  theme: ThemeMode;
  fontSize: number;
  wordWrap: boolean;
  autoCheck: boolean;
  animations: boolean;
}

interface SettingsContextValue {
  settings: Settings;
  resolvedTheme: 'light' | 'dark';
  update: <K extends keyof Settings>(key: K, value: Settings[K]) => void;
  reset: () => void;
}

const DEFAULTS: Settings = {
  theme: 'light',
  fontSize: 13.5,
  wordWrap: true,
  autoCheck: true,
  animations: true,
};

const STORAGE_KEY = 'mpl.studio.settings.v2';
const SettingsContext = createContext<SettingsContextValue | null>(null);

function loadSettings(): Settings {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return DEFAULTS;
    const parsed = JSON.parse(raw) as Partial<Settings>;
    return {
      theme: parsed.theme === 'dark' || parsed.theme === 'system' ? parsed.theme : 'light',
      fontSize: typeof parsed.fontSize === 'number' ? Math.min(20, Math.max(11, parsed.fontSize)) : DEFAULTS.fontSize,
      wordWrap: typeof parsed.wordWrap === 'boolean' ? parsed.wordWrap : DEFAULTS.wordWrap,
      autoCheck: typeof parsed.autoCheck === 'boolean' ? parsed.autoCheck : DEFAULTS.autoCheck,
      animations: typeof parsed.animations === 'boolean' ? parsed.animations : DEFAULTS.animations,
    };
  } catch {
    return DEFAULTS;
  }
}

export function SettingsProvider({ children }: { children: ReactNode }) {
  const [settings, setSettings] = useState<Settings>(loadSettings);
  const [systemDark, setSystemDark] = useState(() =>
    typeof window !== 'undefined' && window.matchMedia
      ? window.matchMedia('(prefers-color-scheme: dark)').matches
      : false,
  );

  useEffect(() => {
    const media = window.matchMedia('(prefers-color-scheme: dark)');
    const onChange = (event: MediaQueryListEvent) => setSystemDark(event.matches);
    media.addEventListener('change', onChange);
    return () => media.removeEventListener('change', onChange);
  }, []);

  const resolvedTheme = settings.theme === 'system' ? (systemDark ? 'dark' : 'light') : settings.theme;

  useEffect(() => {
    const root = document.documentElement;
    root.dataset.theme = resolvedTheme;
    root.style.setProperty('--editor-font-size', `${settings.fontSize}px`);
    root.style.setProperty('color-scheme', resolvedTheme);
    localStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
    if (isTauri()) {
      import('@tauri-apps/api/window')
        .then(({ getCurrentWindow }) => getCurrentWindow().setTheme(resolvedTheme))
        .catch(() => undefined);
    }
  }, [resolvedTheme, settings]);

  useEffect(() => {
    document.documentElement.dataset.motion = settings.animations ? 'on' : 'off';
  }, [settings.animations]);

  const update = useCallback(<K extends keyof Settings>(key: K, value: Settings[K]) => {
    setSettings((current) => ({ ...current, [key]: value }));
  }, []);

  const reset = useCallback(() => setSettings(DEFAULTS), []);

  const value = useMemo(
    () => ({ settings, resolvedTheme, update, reset }),
    [settings, resolvedTheme, update, reset],
  );

  return <SettingsContext.Provider value={value}>{children}</SettingsContext.Provider>;
}

export function useSettings() {
  const context = useContext(SettingsContext);
  if (!context) throw new Error('useSettings must be used inside SettingsProvider');
  return context;
}
