# Alkad2588WH
Читик для игры с ESP, аимботом и триггерботом на DirectX 11.

## Установка
1. Склонируй репо: `git clone https://github.com/YourUsername/Alkad2588WH.git`
2. Убедись, что установлен CMake и Visual Studio (с C++).
3. Залей зависимости:
   ```bash
   git submodule add https://github.com/ocornut/imgui external/imgui
   git submodule add https://github.com/microsoft/Detours external/detours
   ```
4. Сбилдь:
   ```bash
   cmake -S . -B build
   cmake --build build --config Release
   ```
5. Найди `Alkad2588WH.dll` в папке `build/Release` и закинь в игру.

## Зависимости
- ImGui: https://github.com/ocornut/imgui
- Detours: https://github.com/microsoft/Detours

## GitHub Actions
Сборка автоматизирована через `.github/workflows/build.yml`.

## Примечания
- Убедись, что у тебя есть DirectX SDK.
- Если что-то не компилится, пиши в issues!