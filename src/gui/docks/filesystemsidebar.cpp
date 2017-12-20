// For license of this file, see <project-root-folder>/LICENSE.md.

#include "src/gui/docks/filesystemsidebar.h"

#include "gui/plaintoolbutton.h"
#include "miscellaneous/application.h"
#include "miscellaneous/iconfactory.h"
#include "miscellaneous/syntaxhighlighting.h"
#include "miscellaneous/textapplication.h"

#include <QFileSystemModel>
#include <QGroupBox>
#include <QListWidget>
#include <QMimeData>
#include <QVBoxLayout>

FilesystemSidebar::FilesystemSidebar(TextApplication* text_app, QWidget* parent) : DockWidget(parent),
  m_textApp(text_app), m_fsModel(nullptr) {
  setWindowTitle(tr("Filesystem"));
}

Qt::DockWidgetArea FilesystemSidebar::initialArea() const {
  return Qt::DockWidgetArea::LeftDockWidgetArea;
}

bool FilesystemSidebar::initiallyVisible() const {
  return true;
}

int FilesystemSidebar::initialWidth() const {
  return 200;
}

void FilesystemSidebar::load() {
  if (m_fsModel == nullptr) {
    QWidget* widget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(widget);

    m_fsModel = new FileSystemSidebarModel(widget);
    m_fsView = new QListView(widget);
    m_lvFavorites = new FavoritesListWidget(widget);

    layout->setMargin(0);
    widget->setLayout(layout);

    // Initialize toolbar.
    QHBoxLayout* layout_toolbar = new QHBoxLayout(widget);
    PlainToolButton* btn_parent = new PlainToolButton(widget);
    PlainToolButton* btn_add_favorites = new PlainToolButton(widget);

    connect(btn_parent, &PlainToolButton::clicked, this, &FilesystemSidebar::goToParentFolder);
    connect(btn_add_favorites, &PlainToolButton::clicked, this, &FilesystemSidebar::addToFavorites);

    btn_parent->setIcon(qApp->icons()->fromTheme(QSL("go-up")));
    btn_parent->setToolTip(tr("Go to parent folder"));
    btn_add_favorites->setIcon(qApp->icons()->fromTheme(QSL("folder-favorites")));
    btn_add_favorites->setToolTip(tr("Add selected item to favorites"));
    layout_toolbar->addWidget(btn_parent);
    layout_toolbar->addWidget(btn_add_favorites);
    layout_toolbar->addStretch();
    layout_toolbar->setMargin(0);

    // Initialize FS browser
    m_fsModel->setNameFilterDisables(false);
    m_fsModel->setNameFilters(m_textApp->settings()->syntaxHighlighting()->bareFileFilters());
    m_fsView->setDragDropMode(QAbstractItemView::DragDropMode::NoDragDrop);
    m_fsView->setIconSize(QSize(12, 12));
    m_fsView->setModel(m_fsModel);
    m_lvFavorites->setIconSize(QSize(12, 12));
    m_fsModel->setRootPath(QString());
    m_fsView->setRootIndex(m_fsModel->index(qApp->documentsFolder()));

    connect(m_fsView, &QListView::doubleClicked, this, &FilesystemSidebar::openFileFolder);

    // Initialize favorites.
    connect(m_lvFavorites, &QListWidget::doubleClicked, this, &FilesystemSidebar::openFavoriteItem);

    QStringList saved_files = qApp->settings()->value(GROUP(General),
                                                      objectName() + QSL("_files"),
                                                      QStringList()).toStringList();

    foreach (const QString& file, saved_files) {
      m_lvFavorites->loadFileItem(file);
    }

    m_lvFavorites->sortItems(Qt::SortOrder::AscendingOrder);

    layout->addLayout(layout_toolbar, 0);
    layout->addWidget(m_fsView, 1);
    layout->addWidget(m_lvFavorites, 1);

    setWidget(widget);
  }
}

void FilesystemSidebar::addToFavorites() {
  const QFileInfo file_info = m_fsModel->fileInfo(m_fsView->currentIndex());

  if (file_info.isFile() || file_info.isDir()) {
    m_lvFavorites->loadFileItem(QDir::toNativeSeparators(file_info.absoluteFilePath()));
  }

  m_lvFavorites->sortItems(Qt::SortOrder::AscendingOrder);
  saveFavorites();
}

void FilesystemSidebar::openFavoriteItem(const QModelIndex& idx) {
  const auto file_folder = QFileInfo(m_lvFavorites->item(idx.row())->data(Qt::UserRole).toString());

  if (file_folder.isDir()) {
    m_fsView->setRootIndex(m_fsModel->index(file_folder.absoluteFilePath()));
  }
  else {
    emit openFileRequested(file_folder.absoluteFilePath());
  }
}

void FilesystemSidebar::openFileFolder(const QModelIndex& idx) {
  if (m_fsModel->isDir(idx)) {
    m_fsView->setRootIndex(idx);
  }
  else {
    emit openFileRequested(m_fsModel->filePath(idx));
  }
}

void FilesystemSidebar::goToParentFolder() {
  QModelIndex prnt = m_fsView->rootIndex().parent();

  if (prnt.isValid()) {
    m_fsView->setRootIndex(prnt);
  }
  else {
    m_fsView->setRootIndex(m_fsModel->index(QString()));
  }
}

void FilesystemSidebar::saveFavorites() const {
  QStringList favorites;

  for (int i = 0; i < m_lvFavorites->count(); i++) {
    favorites.append(m_lvFavorites->item(i)->data(Qt::UserRole).toString());
  }

  qApp->settings()->setValue(GROUP(General), objectName() + QSL("_files"), favorites);
}

FileSystemSidebarModel::FileSystemSidebarModel(QObject* parent) : QFileSystemModel(parent) {}

FavoritesListWidget::FavoritesListWidget(QWidget* parent) : QListWidget(parent) {
  setDragDropMode(QAbstractItemView::DragDropMode::NoDragDrop);
}

void FavoritesListWidget::loadFileItem(const QString& file_path) {
  QListWidgetItem* item = new QListWidgetItem(this);
  QFileInfo info(file_path);

  item->setData(Qt::UserRole, file_path);
  item->setToolTip(file_path);
  item->setIcon(qApp->icons()->fromTheme(info.isDir() ? QSL("folder") : QSL("gtk-file")));

  if (!info.exists()) {
    item->setText(QFileInfo(file_path).fileName() + tr(" (N/A)"));
    item->setTextColor(Qt::darkRed);
  }
  else {
    item->setText(QFileInfo(file_path).fileName());
  }
}

void FavoritesListWidget::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Delete) {
    event->accept();

    int row = currentRow();

    if (row >= 0) {
      delete takeItem(row);
    }
  }
  else {
    QListWidget::keyPressEvent(event);
  }
}
