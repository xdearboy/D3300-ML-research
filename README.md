# D3300-ML Research v0.3 — Nikon D3300 / аналог Magic Lantern

**Статус: прототип на компьютере + read-only реверс-инжиниринг. Это НЕ готовая прошивка. НИЧЕГО из архива на Nikon D3300 не устанавливать.**

Цель — изучить, возможно ли когда-нибудь загружать собственный ARM-код в Nikon D3300, перехватывать события и добавлять модули (интервалометр, OSD, видеопомощники, LUT). Сейчас этого ещё **нет** на реальной камере.

## Новое в v0.3: проверено на настоящем D3300 C 1.02

В проекте есть чисто Python-декодер `tools/decode_nikon_package.py` для исходного Nikon `D3300_0102.bin`. Он успешно извлёк из предоставленного пользователем файла (`SHA256 0484d43a...ad50152a`) **A** `a860_010101.bin` (786434 байта) и **B** `b860_102.bin` (23854986 байт, SHA256 `c7eb89f1a8e699fdd92c907989682d6d8746a46648436137cbd7e138f9afc3ff`). На настоящей B прошивке проверены карта памяти, reset vector `0x90352FE0`, 23/23 Nikon Hacker patch anchors, экспорт ARM ELF и дизассемблирование загрузочного фрагмента. Полный отчёт: `reports/D3300_C102_static_analysis.md`.

**Самостоятельное повторение на Mac:**

```bash
python3 tools/decode_nikon_package.py ~/Desktop/D3300_0102.bin -o ~/Desktop/d3300-analysis
python3 tools/d3300_recon.py anchors ~/Desktop/d3300-analysis/b860_102.bin --verbose
python3 tools/d3300_elf.py ~/Desktop/d3300-analysis/b860_102.bin -o ~/Desktop/d3300-analysis/d3300_analysis.elf
```

20 синтетических Python-тестов, C-тест меню и отдельно реальный статический анализ. Никаких файлов для обновления фотоаппарата проект не создаёт. Исходные и декодированные проприетарные бинарники Nikon в ZIP не распространяются.

## Что действительно сделано

1. **Ядро модулей C11 на хосте** (`core/`, `modules/`, `sim/`). Интервалометр запускается через абстрактное Camera API, без Nikon. Тестовый прогон даёт пять виртуальных снимков.
2. **Анализатор распакованной B-firmware** (`tools/d3300_lab.py`): начальная ARM-инструкция и адрес reset, карта ROM→RAM C 1.02, строки, точный поиск байтов, diff и экспорт RAM-копий. Ничего не модифицирует.
3. **Проверка известных патчей Nikon Hacker** (`tools/d3300_recon.py`): 23 документированных байтовых участка для B-firmware D3300 C 1.02, из них 20 для Video 64 Mbps и 3 для LiveView timeout. Проверяет исходные/патченные/неизвестные значения. Ничего не патчит.
4. **Поиск ссылок** (`tools/d3300_recon.py refs`): 32-битные little-endian указатели и кандидаты в A32/Thumb-2 `B`, `BL` и `BLX`. Это *эвристика*, не построение полного CFG; косвенные вызовы требуют ручного анализа.
5. **Экспорт в ELF для анализа** (`tools/d3300_elf.py`): генерирует стандартный 32-битный ARM little-endian ELF с B-образом по адресу ROM `0x90020000`; пригоден для `llvm-objdump` и Ghidra. Это **не update image** и **не запускаемый образ для камеры**.
6. **Скрипты для Ghidra** (`ghidra/`): byte-mapped RAM сегменты по Nikon Hacker и закладки на все 23 известные патч-адреса. Изменяют только базу проекта Ghidra.
7. **20 Python-тестов на синтетических образах + C-тест меню**: утилиты, Thumb-2 scanner, ELF, независимые экземпляры модулей и меню прошли проверку.
8. **Read-only разбор событий**: подтверждены RelayEvent/RelayEventEx callback-циклы, классы кнопок и цикл `CSYSCAppButtonController`. Адреса и ограничения записаны в `reports/verified-event-button-paths.json`.
9. **Собственное меню на хосте**: `core/dml_menu.*` предоставляет меню модулей; `make run` сохраняет автоматический прогон, `./build/d3300_sim --interactive` запускает интерактивный режим.

Продолжение анализа кнопок, callback-регистрации и управления графическим буфером описано в `reports/D3300_C102_followup.md`.
Сопоставление объектов ввода, метода `CMENUController` и графических операций приведено в `reports/D3300_C102_menu_graphics.md`.

### Что НЕ сделано

- **Теперь проверено на настоящем файле D3300 C 1.02.** Но это исключительно статический анализ, не исполнение кода на камере.
- Нет никакой возможности подтвердить работу кода на EXPEED 4 без физической камеры или соответствующего эмулятора.
- Нет проверенного безопасного хука, собственного кода внутри штатной прошивки, загрузчика с SD, пользовательского OSD на камере, обработчиков Nikon API или 3D LUT.
- **Нет записи на SD в формате Nikon firmware, нет патча прошивки и нет `.bin` для установки.** Нельзя тестировать ZIP на камере.
- Наличие валидного ELF и адресов в Ghidra не доказывает, что там есть место для безопасного хука. Байтовые позиции патчей — преимущественно константы.

## Быстрый запуск

На Linux/macOS/WSL нужны `python3`, компилятор C11 (`gcc`/`clang`), `make`.

```bash
make test    # Симулятор, C-тест меню и 20 Python-тестов
make run     # 5 виртуальных снимков
./build/d3300_sim --interactive  # меню модулей на хосте
```

## Достать бинарник для настоящего реверса

1. Скачать оригинальную **C 1.02** для Nikon D3300 с [официального сайта Nikon](https://downloadcenter.nikonimglib.com/ru/download/fw/249.html), соблюдая условия Nikon. На странице архив называется `F-D3300-V102W.exe` (Windows) / `F-D3300-V102M.dmg` (Mac), внутри `D3300_0102.bin`. **Не следует делать обновление камеры ради анализа.**
2. Используя исходники [Nikon Hacker](https://github.com/simeonpilgrim/nikon-firmware-tools) и их `Nikon-Patch/LocalUI` → **Decode Firmware**, расшифровать и извлечь B-сегмент. В [оригинальном руководстве](https://simeonpilgrim.com/blog/2021/08/07/getting-started-disassembling-nikon-firmware/) указано имя `b860_102.bin`.
3. Передать файл `b860_102.bin` **на компьютер**, отдельно от любых прошивочных операций. Сохранить SHA-256 и резервную копию исходника.
4. Команды ниже относятся **только** к распакованной B-фирмвари. На исходный `D3300_0102.bin` они намеренно не рассчитаны.

### Аудит известных патч-адресов

```bash
python3 tools/d3300_recon.py anchors b860_102.bin --json audit.json
python3 tools/d3300_recon.py anchors b860_102.bin --verbose
```

`original` = каждый участок группы совпал с эталоном Nikon Hacker; `nikon_hacker_patched` = все участки группы совпали с патченными значениями; `mixed` = часть совпала с исходными, часть с патченными; `unverified` = неожиданные байты/сегменты вне файла. **Даже `original` — не криптографическое подтверждение модели или версии.**

20 адресов для видео и 3 для LiveView берутся из раздела `D3300_0102` в [Nikon Hacker `patches.c`](https://github.com/simeonpilgrim/nikon-firmware-tools/blob/main/Nikon-Patch-JS/src/nikon_patch/patches.c).

### Поиск мест, на которые ссылается ARM-код

```bash
python3 tools/d3300_recon.py refs b860_102.bin --target 0x90352FE0 --mode both
python3 tools/d3300_recon.py refs b860_102.bin --target 0x900271E4 --mode branches --isa thumb
python3 tools/d3300_recon.py refs b860_102.bin --target 0x900292F0 --mode pointers --json refs.json
```

- `0x90352FE0` — пример адреса reset code из статьи автора Nikon Hacker.
- `0x900292F0` — ROM-адрес одного из тайм-аутных патчей, **не доказанный адрес функции или хука**.
- Вывод ссылок — **кандидаты**; нужны дизассемблер и проверка control-flow.

### ELF для Ghidra и LLVM

```bash
python3 tools/d3300_elf.py b860_102.bin -o d3300_analysis.elf
readelf -h d3300_analysis.elf
llvm-objdump -d --start-address=0x90352FE0 --stop-address=0x90353060 d3300_analysis.elf
```

ELF самостоятельно создан утилитой: один RX ROM-сегмент по адресу `0x90020000`, CPU ARM, ABI EABI5. **Вся ROM отмечена как код**, поскольку у нас нет достоверной карты всех внутренних data/code сегментов. Дизассемблер может ошибочно интерпретировать данные как инструкции. ELF не совместим с firmware updater Nikon.

В Ghidra можно импортировать `.elf` как ELF или `b860_102.bin` как `Raw Binary` с базой `0x90020000`, ARMv7 little-endian. Перед Auto Analyze запустить:

1. `ghidra/D3300_Map_RAM.py` — отобразить известные ROM-источники RAM-копий как byte-mapped RAM.
2. `ghidra/D3300_Mark_NikonHacker_Anchors.py` — проставить информационные закладки на 23 адреса с проверкой ожидаемых байтов.
3. `ghidra/D3300_Mark_Event_Button_Candidates.py` — отметить девять подтверждённых узлов событий и кнопок.

Все три скрипта проверены headless-запуском Ghidra 12.2 на настоящем B-образе с SHA-256 `c7eb89f1...9afc3ff`. Они изменяют базу проекта Ghidra, поэтому сохраняйте резервную копию базы.

### Существующий read-only анализатор v0.1

```bash
python3 tools/d3300_lab.py inspect b860_102.bin --json map.json
python3 tools/d3300_lab.py extract-ram b860_102.bin -o ram_export
python3 tools/d3300_lab.py find-bytes b860_102.bin "18 f0 9f e5"
python3 tools/d3300_lab.py strings b860_102.bin --contains Nikon
python3 tools/d3300_lab.py diff b860_102.bin other_b_image.bin
```

## Карта ROM → RAM Nikon D3300 C 1.02

| ROM source | RAM target | Bytes | Тип |
|---|---|---:|---|
| `0x9158A358` | `0x10000000` | `0x3B5B4` | rwx, kernel |
| `0x915C590C` | `0x1003B5B4` | `0x179C` | rw |
| `0x916A73A0` | `0x1011D420` | `0x28B3C` | rw |
| `0x916A739C` | `0x1011D03C` | `0x4` | rw |

Источник: [Nikon Hacker FirmwareNotes — D3300](https://github.com/simeonpilgrim/nikon-firmware-tools/blob/main/FirmwareNotes/FirmwareNotes.md).

## Ближайший реальный технический рубеж

1. Получить и верифицировать декодированный B-бинарник C 1.02, получить SHA-256, проверить 23 эталонных адреса.
2. Дизассемблировать reset `0x90352FE0` и соответствующий загрузке RAM-копий код. Изучить границы iTRON kernel, память и вызовы RTOS.
3. Связать обработчики с физическими MENU/OK и найти вывод на LCD/OSD.
4. Спроектировать read-only экспериментальный ARM-хук и безопасный recovery-процесс отдельно. До этого **прошивать камеру нельзя**.

## Авторские материалы

- Nikon Hacker, инструменты: https://github.com/simeonpilgrim/nikon-firmware-tools
- Nikon Hacker, карта адресов: https://github.com/simeonpilgrim/nikon-firmware-tools/blob/main/FirmwareNotes/FirmwareNotes.md
- Nikon Hacker, точные D3300 C1.02 patch bytes/offsets: https://github.com/simeonpilgrim/nikon-firmware-tools/blob/main/Nikon-Patch-JS/src/nikon_patch/patches.c
- Simeon Pilgrim, Ghidra tutorial specifically on D3300 C1.02: https://simeonpilgrim.com/blog/2021/08/07/getting-started-disassembling-nikon-firmware/
- Nikon official firmware C 1.02: https://downloadcenter.nikonimglib.com/ru/download/fw/249.html

Это независимый исследовательский код. Проприетарные бинарники Nikon и материалы Nikon Hacker не включены в архив.
