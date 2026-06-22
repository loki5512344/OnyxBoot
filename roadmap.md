# Roadmap

## v0.2 - OC2r (VirtIO)
- [x] UART драйвер (NS16550A, адрес из FDT)
- [x] VirtIO v2 MMIO block (probe, read_sector)
- [x] ELF64 парсер (header, program headers, load)
- [x] FDT парсер (memory, UART, VirtIO)
- [x] Точка входа _start (BSS, стек, прыжок)
- [x] Пофиксить vring адреса - Used ring в 0x090 вместо 0x0A0
- [ ] Проверить на QEMU: собрать образ с kernel.elf
- [ ] Проверить на OC2r/Sedna: реальный FDT

## v0.3 - Milk-V Duo S (SDHCI)
- [x] SDHCI драйвер - чтение SD-карты, инициализация контроллера
- [x] Milk-V DTB - .dts для Duo S (dts/milkv-duos.dts)
- [x] DTB от предыдущей стадии (a1), единая сборка
- [x] Загрузка kernel.elf с SD-карты по LBA
- [ ] Проверить на реальной Milk-V Duo S

## v0.4 - Стабильность
- [x] Boot menu через UART (выбор устройства)
- [x] Поддержка нескольких VirtIO/SD устройств
- [x] Проверка целостности ELF (check_safe)
- [x] Прогресс-бар загрузки

## v0.5 - SlipperBoot standalone
- [ ] Свой образ диска (genimage или скрипт)
- [ ] Поддержка 9P (VirtIO filesystem) для отладки
- [ ] Debug UART (вывод FDT, регистров)
- [ ] CI сборка (GitHub Actions)

## v1.0 - Релиз
- [ ] Полная документация
- [ ] Проверка на OC2r + Milk-V Duo S
- [ ] Стабильная интеграция с SlipperOS
