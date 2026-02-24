#pragma once

#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVector>
#include <QWidget>

class AnimatedTabBar : public QWidget {
  Q_OBJECT
  Q_PROPERTY(int highlightX READ highlightX WRITE setHighlightX)

public:
  explicit AnimatedTabBar(QWidget *parent = nullptr);

  int addTab(const QString &text);
  void setCurrentIndex(int idx);
  int currentIndex() const { return m_current; }

  int highlightX() const { return m_hlX; }
  void setHighlightX(int x);

signals:
  void currentChanged(int index);

protected:
  void resizeEvent(QResizeEvent *e) override;
  void paintEvent(QPaintEvent *e) override;

private:
  void updateHighlight(bool animate);

  QHBoxLayout *m_layout = nullptr;
  QVector<QPushButton *> m_tabs;
  int m_current = 0;
  int m_hlX = 0;
  int m_hlWidth = 0;

  QPropertyAnimation *m_anim = nullptr;
};
