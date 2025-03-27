/**
 * @file screen_list_view.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-26
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <QListWidget>

class ScreenListView : public QListWidget {
    Q_OBJECT
public:
    enum ScreenListViewRole
    {
        eScreenIdRole = Qt::UserRole + 1,
        eScreenWidthRole,
        eScreenHeightRole,
    };

public:
    ScreenListView(QWidget *parent = nullptr);

    static QListWidgetItem *make_screen_item(const QString &name, int screenId, int width,
                                             int height);
    static QListWidgetItem *make_screen_item(const QString &name, int screenId, QSize screenSize);

    void startDrag(Qt::DropActions supportedActions) override;
};