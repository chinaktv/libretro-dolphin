// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include <QAbstractTableModel>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "Core/TitleDatabase.h"

#include "DolphinQt/GameList/GameTracker.h"

namespace UICommon
{
class GameFile;
}

class GameListModel final : public QAbstractTableModel
{
  Q_OBJECT

public:
  explicit GameListModel(QObject* parent = nullptr);

  // Qt's Model/View stuff uses these overrides.
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;

  std::shared_ptr<const UICommon::GameFile> GetGameFile(int index) const;
  // Path of the game at the specified index.
  QString GetPath(int index) const;
  // Unique identifier of the game at the specified index.
  QString GetUniqueIdentifier(int index) const;
  bool ShouldDisplayGameListItem(int index) const;
  void SetSearchTerm(const QString& term);

  enum
  {
    COL_PLATFORM = 0,
    COL_BANNER,
    COL_TITLE,
    COL_DESCRIPTION,
    COL_MAKER,
    COL_ID,
    COL_COUNTRY,
    COL_SIZE,
    COL_FILE_NAME,
    COL_TAGS,
    NUM_COLS
  };

  void AddGame(const std::shared_ptr<const UICommon::GameFile>& game);
  void UpdateGame(const std::shared_ptr<const UICommon::GameFile>& game);
  void RemoveGame(const std::string& path);

  void SetScale(float scale);
  float GetScale() const;

  const QStringList& GetAllTags() const;
  const QStringList GetGameTags(const std::string& path) const;

  void AddGameTag(const std::string& path, const QString& name);
  void RemoveGameTag(const std::string& path, const QString& name);

  void NewTag(const QString& name);
  void DeleteTag(const QString& name);

  void PurgeCache();

private:
  // Index in m_games, or -1 if it isn't found
  int FindGame(const std::string& path) const;

  QStringList m_tag_list;
  QMap<QString, QVariant> m_game_tags;

  GameTracker m_tracker;
  QList<std::shared_ptr<const UICommon::GameFile>> m_games;
  Core::TitleDatabase m_title_database;
  QString m_term;
  float m_scale = 1.0;
};