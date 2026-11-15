# Источники визуальных референсов (состояние на ноябрь 2026)

- Официальный список команд F1 2026: https://www.formula1.com/en/teams
- Общий обзор 2026 логотипов (в том числе чёрно-белые версии): https://mark-boots.github.io/f1-2026-assets/
- Категория исторических и текущих логотипов команд Wikimedia Commons: https://commons.wikimedia.org/wiki/Category:Formula_One_constructor_logos
- Пример лицензирования Mercedes 2026: https://commons.wikimedia.org/wiki/File:Mercedes-AMG_Petronas_F1_Team_logo_(2026).svg
- Пример McLaren Speedmark: https://commons.wikimedia.org/wiki/File:McLaren_Speedmark.svg
- Пример Haas 2026: https://commons.wikimedia.org/wiki/File:TGR_Haas_F1_Team_Logo_(2026).svg

Встроенные 48x32 знаки — вручную нарисованные упрощённые адаптации узнаваемых форм, а **не точные копии** этих файлов. На монохромном экране 128x64 неизбежны упрощения, особенно Ferrari, Red Bull, Cadillac и Racing Bulls. Логотипы и названия могут защищаться как торговые марки; при публичной публикации игры следует отдельно проверить права на использование.

Русский пиксельный набор сгенерирован из растровых изображений символов DejaVu Sans Mono Bold (DejaVu Fonts project). Из исходного файла шрифта в архив ничего не помещается — только конечные массивы битовых глифов внутри C-кода игры.

## Ferrari в v0.5.2

Пользователь предоставил пиксельную схему `90bd7caccd31e6c34f184effe2d888f6.jpg` (ссылка Google Share не открылась; локальный файл найден в Downloads). Чёрные клетки схемы извлечены в `tools/ferrari_reference_mask.txt`; `tools/refine_badges.py` уменьшает щит с сохранением пропорций до 24×30 внутри 48×32. Цветные полосы не переносятся. Это адаптация предоставленного референса для монохромного экрана, а не новый рисунок коня.
