// For license of this file, see <project-root-folder>/LICENSE.md.

#include "external-tools/externaltools.h"

#include "external-tools/externaltool.h"
#include "external-tools/predefinedtools.h"
#include "gui/docks/outputwindow.h"
#include "miscellaneous/application.h"
#include "miscellaneous/settings.h"
#include "miscellaneous/textapplication.h"
#include "network-web/networkfactory.h"

#include <functional>
#include <QDir>
#include <QInputDialog>

#include <QAction>
#include <QClipboard>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPointer>
#include <QRegularExpression>

ExternalTools::ExternalTools(TextApplication* parent)
  : QObject(parent), m_application(parent), m_predefinedTools(QList<PredefinedTool*>()),
  m_customTools(QList<ExternalTool*>()) {
  loadPredefinedTools();
}

ExternalTools::~ExternalTools() {
  qDeleteAll(m_customTools);
  qDeleteAll(m_predefinedTools);
  qDebug("Destroying ExternalTools.");
}

QList<QAction*> ExternalTools::generateToolsMenuTools(QWidget* parent) const {
  QList<QAction*> actions;
  QMap<QString, QMenu*> categories;

  foreach (ExternalTool* tool, m_customTools) {
    QAction* act = new QAction(tool->name(), parent);

    if (!tool->category().isEmpty()) {
      if (!categories.contains(tool->category())) {
        QMenu* category_menu = new QMenu(parent);

        category_menu->setTitle(tool->category());

        actions.append(category_menu->menuAction());
        categories.insert(tool->category(), category_menu);
      }

      categories[tool->category()]->addAction(act);
    }
    else {
      actions.append(act);
    }

    act->setData(QVariant::fromValue(tool));
    act->setShortcut(QKeySequence::fromString(tool->shortcut(), QKeySequence::SequenceFormat::PortableText));
    act->setShortcutContext(Qt::ApplicationShortcut);

    connect(act, &QAction::triggered, this, &ExternalTools::runSelectedExternalTool);
  }

  // We add already existing persistent actions for
  // built-in tools to the "Tools" menu too.
  foreach (PredefinedTool* tool, m_predefinedTools) {
    if (tool->addToEditMenu()) {
      continue;
    }

    if (!tool->category().isEmpty()) {
      if (!categories.contains(tool->category())) {
        QMenu* category_menu = new QMenu(parent);

        category_menu->setTitle(tool->category());
        actions.append(category_menu->menuAction());
        categories.insert(tool->category(), category_menu);
      }

      categories[tool->category()]->addAction(tool->action());
    }
    else {
      actions.append(tool->action());
    }
  }

  return actions;
}

QList<QAction*> ExternalTools::generateEditMenuTools(QWidget* parent) const {
  QList<QAction*> actions;
  QMap<QString, QMenu*> categories;

  // We add already existing persistent actions for
  // built-in tools to the "Tools" menu too.
  foreach (PredefinedTool* tool, m_predefinedTools) {
    if (!tool->addToEditMenu()) {
      continue;
    }

    if (!tool->category().isEmpty()) {
      if (!categories.contains(tool->category())) {
        QMenu* category_menu = new QMenu(parent);

        category_menu->setTitle(tool->category());
        actions.append(category_menu->menuAction());
        categories.insert(tool->category(), category_menu);
      }

      categories[tool->category()]->addAction(tool->action());
    }
    else {
      actions.append(tool->action());
    }
  }

  return actions;
}

QList<ExternalTool*> ExternalTools::tools() const {
  return m_customTools;
}

QList<QAction*> ExternalTools::predefinedToolsActions() const {
  QList<QAction*> act;

  foreach (PredefinedTool* tool, m_predefinedTools) {
    act.append(tool->action());
  }

  return act;
}

void ExternalTools::saveExternalTools(const QList<ExternalTool*>& ext_tools) {
  // We persistently save custom tools.
  QSettings sett_ext_tools(qApp->settings()->pathName() + QDir::separator() + EXT_TOOLS_CONFIG, QSettings::Format::IniFormat);

  sett_ext_tools.clear();
  int i = 0;

  foreach (const ExternalTool* tool, ext_tools) {
    if (tool->isPredefined()) {
      continue;
    }

    sett_ext_tools.beginGroup(QString::number(i++));

    sett_ext_tools.setValue(QSL("interpreter"), tool->interpreter());
    sett_ext_tools.setValue(QSL("name"), tool->name());
    sett_ext_tools.setValue(QSL("script"), tool->script().toUtf8());
    sett_ext_tools.setValue(QSL("input"), int(tool->input()));
    sett_ext_tools.setValue(QSL("output"), int(tool->output()));
    sett_ext_tools.setValue(QSL("category"), tool->category());
    sett_ext_tools.setValue(QSL("shortcut"), tool->shortcut());
    sett_ext_tools.setValue(QSL("prompt"), tool->prompt());

    sett_ext_tools.endGroup();
  }
}

void ExternalTools::runSelectedExternalTool() {
  TextEditor* editor = m_application->currentEditor();

  if (editor != nullptr) {
    ExternalTool* tool_to_run = qobject_cast<QAction*>(sender())->data().value<ExternalTool*>();

    connect(tool_to_run, &ExternalTool::toolFinished, this, &ExternalTools::onToolFinished,
            Qt::ConnectionType(Qt::ConnectionType::AutoConnection | Qt::ConnectionType::UniqueConnection));
    connect(tool_to_run, &ExternalTool::partialOutputObtained, this, &ExternalTools::onToolPartialOutputObtained,
            Qt::ConnectionType(Qt::ConnectionType::AutoConnection | Qt::ConnectionType::UniqueConnection));

    // We run the tool.
    runTool(tool_to_run, editor);
  }
}

void ExternalTools::loadPredefinedTools() {
  PredefinedTool* insert_date_time = new PredefinedTool(&PredefinedTools::currentDateTime, this);

  insert_date_time->setActionObjectName(QSL("m_actionPredefCurrDateTime"));
  insert_date_time->setCategory(tr("Insert..."));
  insert_date_time->setName(tr("Date/time"));
  insert_date_time->setInput(ToolInput::NoInput);
  insert_date_time->setOutput(ToolOutput::InsertAtCursorPosition);

  m_predefinedTools.append(insert_date_time);

  PredefinedTool* json_beautify = new PredefinedTool(&PredefinedTools::jsonBeautify, this);

  json_beautify->setActionObjectName(QSL("m_actionPredefJsonBeautify"));
  json_beautify->setCategory(tr("JSON"));
  json_beautify->setName(tr("Beautify"));
  json_beautify->setInput(ToolInput::SelectionDocument);
  json_beautify->setOutput(ToolOutput::ReplaceSelectionDocument);

  m_predefinedTools.append(json_beautify);

  PredefinedTool* json_minify = new PredefinedTool(&PredefinedTools::jsonMinify, this);

  json_minify->setActionObjectName(QSL("m_actionPredefMinify"));
  json_minify->setCategory(tr("JSON"));
  json_minify->setName(tr("Minify"));
  json_minify->setInput(ToolInput::SelectionDocument);
  json_minify->setOutput(ToolOutput::ReplaceSelectionDocument);

  m_predefinedTools.append(json_minify);

  PredefinedTool* xml_check = new PredefinedTool(&PredefinedTools::xmlCheck, this);

  xml_check->setActionObjectName(QSL("m_actionPredefXmlCheck"));
  xml_check->setCategory(tr("XML"));
  xml_check->setName(tr("Check XML syntax"));
  xml_check->setInput(ToolInput::SelectionDocument);
  xml_check->setOutput(ToolOutput::DumpToOutputWindow);

  m_predefinedTools.append(xml_check);

  PredefinedTool* xml_beautify = new PredefinedTool(&PredefinedTools::xmlBeautify, this);

  xml_beautify->setActionObjectName(QSL("m_actionPredefXmlBeautify"));
  xml_beautify->setCategory(tr("XML"));
  xml_beautify->setName(tr("Beautify"));
  xml_beautify->setInput(ToolInput::SelectionDocument);
  xml_beautify->setOutput(ToolOutput::ReplaceSelectionDocument);

  m_predefinedTools.append(xml_beautify);

  PredefinedTool* xml_linearize = new PredefinedTool(&PredefinedTools::xmlLinearize, this);

  xml_linearize->setActionObjectName(QSL("m_actionPredefMinify"));
  xml_linearize->setCategory(tr("XML"));
  xml_linearize->setName(tr("Linearize/Minfy"));
  xml_linearize->setInput(ToolInput::SelectionDocument);
  xml_linearize->setOutput(ToolOutput::ReplaceSelectionDocument);

  m_predefinedTools.append(xml_linearize);

  PredefinedTool* tobase64 = new PredefinedTool(&PredefinedTools::toBase64, this);

  tobase64->setActionObjectName(QSL("m_actionPredefToBase64"));
  tobase64->setCategory(tr("MIME tools"));
  tobase64->setName(tr("Text → Base64"));
  tobase64->setInput(ToolInput::SelectionDocument);
  tobase64->setOutput(ToolOutput::ReplaceSelectionDocument);

  m_predefinedTools.append(tobase64);

  PredefinedTool* tobase64url = new PredefinedTool(&PredefinedTools::toBase64Url, this);

  tobase64url->setActionObjectName(QSL("m_actionPredefToBase64Url"));
  tobase64url->setCategory(tr("MIME tools"));
  tobase64url->setName(tr("Text → Base64Url"));
  tobase64url->setInput(ToolInput::SelectionDocument);
  tobase64url->setOutput(ToolOutput::ReplaceSelectionDocument);

  m_predefinedTools.append(tobase64url);

  PredefinedTool* tohtmlencoded = new PredefinedTool(&PredefinedTools::toHtmlEscaped, this);

  tohtmlencoded->setActionObjectName(QSL("m_actionPredefToHtmlEscaped"));
  tohtmlencoded->setCategory(tr("MIME tools"));
  tohtmlencoded->setName(tr("Text → HTML escaped"));
  tohtmlencoded->setInput(ToolInput::SelectionDocument);
  tohtmlencoded->setOutput(ToolOutput::ReplaceSelectionDocument);

  m_predefinedTools.append(tohtmlencoded);

  PredefinedTool* tourlencoded = new PredefinedTool(&PredefinedTools::toUrlEncoded, this);

  tourlencoded->setActionObjectName(QSL("m_actionPredefToUrlEncoded"));
  tourlencoded->setCategory(tr("MIME tools"));
  tourlencoded->setName(tr("Text → URL encoded"));
  tourlencoded->setInput(ToolInput::SelectionDocument);
  tourlencoded->setOutput(ToolOutput::ReplaceSelectionDocument);

  m_predefinedTools.append(tourlencoded);

  PredefinedTool* tolower = new PredefinedTool(&PredefinedTools::toLower, this);

  tolower->setActionObjectName(QSL("m_actionPredefToLower"));
  tolower->setCategory(tr("Text case conversion"));
  tolower->setName(tr("to lower case"));
  tolower->setInput(ToolInput::SelectionDocument);
  tolower->setOutput(ToolOutput::ReplaceSelectionDocument);
  tolower->setAddToEditMenu(true);

  m_predefinedTools.append(tolower);

  PredefinedTool* toupper = new PredefinedTool(&PredefinedTools::toUpper, this);

  toupper->setActionObjectName(QSL("m_actionPredefToUpper"));
  toupper->setCategory(tr("Text case conversion"));
  toupper->setName(tr("TO UPPER CASE"));
  toupper->setInput(ToolInput::SelectionDocument);
  toupper->setOutput(ToolOutput::ReplaceSelectionDocument);
  toupper->setAddToEditMenu(true);

  m_predefinedTools.append(toupper);

  PredefinedTool* tosentence = new PredefinedTool(&PredefinedTools::toSentenceCase, this);

  tosentence->setActionObjectName(QSL("m_actionPredefToSentence"));
  tosentence->setCategory(tr("Text case conversion"));
  tosentence->setName(tr("To sentence case"));
  tosentence->setInput(ToolInput::SelectionDocument);
  tosentence->setOutput(ToolOutput::ReplaceSelectionDocument);
  tosentence->setAddToEditMenu(true);

  m_predefinedTools.append(tosentence);

  PredefinedTool* totitle = new PredefinedTool(&PredefinedTools::toTitleCase, this);

  totitle->setActionObjectName(QSL("m_actionPredefToTitle"));
  totitle->setCategory(tr("Text case conversion"));
  totitle->setName(tr("To Title Case"));
  totitle->setInput(ToolInput::SelectionDocument);
  totitle->setOutput(ToolOutput::ReplaceSelectionDocument);
  totitle->setAddToEditMenu(true);

  m_predefinedTools.append(totitle);

  PredefinedTool* toinvert = new PredefinedTool(&PredefinedTools::invertCase, this);

  toinvert->setActionObjectName(QSL("m_actionPredefInvertCase"));
  toinvert->setCategory(tr("Text case conversion"));
  toinvert->setName(tr("Invert case"));
  toinvert->setInput(ToolInput::SelectionDocument);
  toinvert->setOutput(ToolOutput::ReplaceSelectionDocument);
  toinvert->setAddToEditMenu(true);

  m_predefinedTools.append(toinvert);

  PredefinedTool* frombase64 = new PredefinedTool(&PredefinedTools::fromBase64, this);

  frombase64->setActionObjectName(QSL("m_actionPredefFromBase64"));
  frombase64->setCategory(tr("MIME tools"));
  frombase64->setName(tr("Base64 → text"));
  frombase64->setInput(ToolInput::SelectionDocument);
  frombase64->setOutput(ToolOutput::ReplaceSelectionDocument);

  m_predefinedTools.append(frombase64);

  PredefinedTool* frombase64url = new PredefinedTool(&PredefinedTools::fromBase64Url, this);

  frombase64url->setActionObjectName(QSL("m_actionPredefFromBase64Url"));
  frombase64url->setCategory(tr("MIME tools"));
  frombase64url->setName(tr("Base64Url → text"));
  frombase64url->setInput(ToolInput::SelectionDocument);
  frombase64url->setOutput(ToolOutput::ReplaceSelectionDocument);

  m_predefinedTools.append(frombase64url);

  PredefinedTool* fromurlencoded = new PredefinedTool(&PredefinedTools::fromUrlEncoded, this);

  fromurlencoded->setActionObjectName(QSL("m_actionPredefFromUrlEncoded"));
  fromurlencoded->setCategory(tr("MIME tools"));
  fromurlencoded->setName(tr("URL encoded → text"));
  fromurlencoded->setInput(ToolInput::SelectionDocument);
  fromurlencoded->setOutput(ToolOutput::ReplaceSelectionDocument);

  m_predefinedTools.append(fromurlencoded);

  PredefinedTool* send_to_clbin = new PredefinedTool(&PredefinedTools::sendToClbin, this);

  send_to_clbin->setActionObjectName(QSL("m_actionPredefSendClbin"));
  send_to_clbin->setCategory(tr("Upload to..."));
  send_to_clbin->setName(tr("clbin.com"));
  send_to_clbin->setInput(ToolInput::SelectionDocument);
  send_to_clbin->setOutput(ToolOutput::DumpToOutputWindow);

  m_predefinedTools.append(send_to_clbin);

  PredefinedTool* send_to_github = new PredefinedTool(&PredefinedTools::sendToGithub, this);

  send_to_github->setActionObjectName(QSL("m_actionPredefSendGithub"));
  send_to_github->setCategory(tr("Upload to..."));
  send_to_github->setName(tr("github.com"));
  send_to_github->setInput(ToolInput::SelectionDocument);
  send_to_github->setOutput(ToolOutput::DumpToOutputWindow);

  m_predefinedTools.append(send_to_github);

  PredefinedTool* send_to_ixio = new PredefinedTool(&PredefinedTools::sendToIxio, this);

  send_to_ixio->setActionObjectName(QSL("m_actionPredefSendIxio"));
  send_to_ixio->setCategory(tr("Upload to..."));
  send_to_ixio->setName(tr("ix.io"));
  send_to_ixio->setInput(ToolInput::SelectionDocument);
  send_to_ixio->setOutput(ToolOutput::DumpToOutputWindow);

  m_predefinedTools.append(send_to_ixio);

  PredefinedTool* send_to_sprunge = new PredefinedTool(&PredefinedTools::sendToSprunge, this);

  send_to_sprunge->setActionObjectName(QSL("m_actionPredefSendSprunge"));
  send_to_sprunge->setCategory(tr("Upload to..."));
  send_to_sprunge->setName(tr("sprunge.us"));
  send_to_sprunge->setInput(ToolInput::SelectionDocument);
  send_to_sprunge->setOutput(ToolOutput::DumpToOutputWindow);

  m_predefinedTools.append(send_to_sprunge);

  // We pre-generate actions for built-in tools.
  foreach (PredefinedTool* tool, m_predefinedTools) {
    QAction* act = new QAction(tool->name(), tool);

    act->setObjectName(tool->actionObjectName());
    act->setData(QVariant::fromValue(tool));
    act->setShortcut(QKeySequence::fromString(tool->shortcut(), QKeySequence::SequenceFormat::PortableText));
    act->setShortcutContext(Qt::ApplicationShortcut);

    tool->setAction(act);

    connect(act, &QAction::triggered, this, &ExternalTools::runSelectedExternalTool);
  }
}

void ExternalTools::loadCustomTools() {
  qDeleteAll(m_customTools);
  m_customTools.clear();

  QSettings sett_ext_tools(qApp->settings()->pathName() + QDir::separator() + EXT_TOOLS_CONFIG, QSettings::Format::IniFormat);
  QStringList sections = sett_ext_tools.childGroups();

  foreach (const QString& section, sections) {
    sett_ext_tools.beginGroup(section);

    ExternalTool* tool = new ExternalTool(this);

    tool->setInterpreter(sett_ext_tools.value(QSL("interpreter"), QSL(EXT_TOOL_INTERPRETER)).toString());
    tool->setName(sett_ext_tools.value(QSL("name")).toString());
    tool->setScript(sett_ext_tools.value(QSL("script")).toString());
    tool->setPrompt(sett_ext_tools.value(QSL("prompt")).toString());
    tool->setInput(ToolInput(sett_ext_tools.value(QSL("input"), int(ToolInput::SelectionDocument)).toInt()));
    tool->setOutput(ToolOutput(sett_ext_tools.value(QSL("output"), int(ToolOutput::ReplaceSelectionDocument)).toInt()));
    tool->setCategory(sett_ext_tools.value(QSL("category")).toString());
    tool->setShortcut(sett_ext_tools.value(QSL("shortcut")).toString());

    m_customTools.append(tool);

    sett_ext_tools.endGroup();
  }

  // We add extra tools if this is the first time when app runs.
  if (m_customTools.isEmpty()) {
    ExternalTool* ext_bash_xml = new ExternalTool(this);

    ext_bash_xml->setScript("IFS=''"
                            "read -r fil"
                            "xmllint --format \"$fil\" > \"${fil}.out\" 2> /dev/null"
                            "mv \"${fil}.out\" \"$fil\"");
    ext_bash_xml->setCategory(tr("Bash (external tool examples)"));
    ext_bash_xml->setInput(ToolInput::SavedFile);
    ext_bash_xml->setOutput(ToolOutput::ReloadFile);
    ext_bash_xml->setName("XML - beautify");

    m_customTools.append(ext_bash_xml);

    ExternalTool* ext_bash_json = new ExternalTool(this);

    ext_bash_json->setScript("import sys, json;\n\ndata = json.load(sys.stdin)\nprint(json.dumps(data, indent=2))");
    ext_bash_json->setCategory(tr("Python (external tool examples)"));
    ext_bash_json->setInterpreter(QSL("python3.6"));
    ext_bash_json->setInput(ToolInput::SelectionDocument);
    ext_bash_json->setOutput(ToolOutput::ReplaceSelectionDocument);
    ext_bash_json->setName("JSON - beautify");

    m_customTools.append(ext_bash_json);

    ExternalTool* ext_bash_sha256 = new ExternalTool(this);

    ext_bash_sha256->setScript("sha256sum | head -c 64");
    ext_bash_sha256->setCategory(tr("Bash (external tool examples)"));
    ext_bash_sha256->setInput(ToolInput::SelectionDocument);
    ext_bash_sha256->setOutput(ToolOutput::ReplaceSelectionDocument);
    ext_bash_sha256->setName(tr("Get SHA256 sum of selected/all text"));

    m_customTools.append(ext_bash_sha256);

    ExternalTool* ext_python_reverse = new ExternalTool(this);

    ext_python_reverse->setScript("print raw_input().lower()[::-1]");
    ext_python_reverse->setInterpreter(QSL("python3.6"));
    ext_python_reverse->setCategory(tr("Python (external tool examples)"));
    ext_python_reverse->setInput(ToolInput::CurrentLine);
    ext_python_reverse->setOutput(ToolOutput::ReplaceCurrentLine);
    ext_python_reverse->setName(tr("Reverse current line"));

    m_customTools.append(ext_python_reverse);

    ExternalTool* ext_bash_seq = new ExternalTool(this);

    ext_bash_seq->setScript("IFS=' '\nread -r a b\nunset IFS\nfor i in $(seq $a $b); do printf \"$i \"; done");
    ext_bash_seq->setCategory(tr("Bash (external tool examples)"));
    ext_bash_seq->setPrompt(tr("Enter sequence bounds (for example \"0 10\"):"));
    ext_bash_seq->setInput(ToolInput::AskForInput);
    ext_bash_seq->setOutput(ToolOutput::InsertAtCursorPosition);
    ext_bash_seq->setName(tr("Generate sequence"));

    m_customTools.append(ext_bash_seq);

    ExternalTool* ext_python_eval = new ExternalTool(this);

    ext_python_eval->setScript("import sys\nimport math\n\nprint(eval(sys.stdin.read()))");
    ext_python_eval->setCategory(tr("Python (external tool examples)"));
    ext_python_eval->setInterpreter(QSL("python3.6"));
    ext_python_eval->setPrompt(tr("Enter Python code:"));
    ext_python_eval->setInput(ToolInput::AskForInput);
    ext_python_eval->setOutput(ToolOutput::InsertAtCursorPosition);
    ext_python_eval->setName(tr("Run Python code"));

    m_customTools.append(ext_python_eval);

    ExternalTool* ext_bash_garbage = new ExternalTool(this);

    ext_bash_garbage->setScript("read -r count\n\n"
                                "tr -dc a-z1-4 </dev/urandom | tr 1-2 ' \n' | awk 'length==0 || length>50' | tr 3-4 ' ' | sed 's/^ *//' | cat -s | sed 's/ / /g' | fmt | head -n $count");
    ext_bash_garbage->setCategory(tr("Bash (external tool examples)"));
    ext_bash_garbage->setPrompt(tr("Enter number of lines:"));
    ext_bash_garbage->setInput(ToolInput::AskForInput);
    ext_bash_garbage->setOutput(ToolOutput::InsertAtCursorPosition);
    ext_bash_garbage->setName(tr("Generate garbage text"));

    m_customTools.append(ext_bash_garbage);

    ExternalTool* ext_bash_exec = new ExternalTool(this);

    ext_bash_exec->setScript("IFS=''\n"
                             "read -r fil\n"
                             "eval $fil");
    ext_bash_exec->setCategory(tr("Bash (external tool examples)"));
    ext_bash_exec->setPrompt(tr("Enter Bash code:"));
    ext_bash_exec->setInput(ToolInput::AskForInput);
    ext_bash_exec->setOutput(ToolOutput::InsertAtCursorPosition);
    ext_bash_exec->setName(tr("Run Bash code"));

    m_customTools.append(ext_bash_exec);
  }
}

void ExternalTools::reloadTools() {
  loadCustomTools();
  emit externalToolsChanged();
}

void ExternalTools::runTool(ExternalTool* tool_to_run, TextEditor* editor) {
  if (tool_to_run->isRunning()) {
    m_application->outputWindow()->displayOutput(OutputSource::Application,
                                                 tr("Tool '%1' is already running.").arg(tool_to_run->name()),
                                                 QMessageBox::Icon::Warning);
  }
  else {

    QPointer<TextEditor> ptr_editor = editor;
    QString data;

    switch (tool_to_run->input()) {
      case ToolInput::SelectionDocument:
        data = !ptr_editor->selectionEmpty() ? ptr_editor->getSelText() : ptr_editor->getText(ptr_editor->length() + 1);
        break;

      case ToolInput::AskForInput: {
        bool ok;

        data = QInputDialog::getText(qApp->mainFormWidget(), tr("Enter input for external tool"),
                                     tool_to_run->prompt(), QLineEdit::EchoMode::Normal, QString(), &ok);

        if (!ok) {
          return;
        }

        break;
      }

      case ToolInput::CurrentLine:
        data = ptr_editor->getCurLine(-1);
        break;

      case ToolInput::SavedFile:
        bool ok;

        ptr_editor->save(&ok);
        data = ptr_editor->filePath();
        break;

      case ToolInput::NoInput:
      default:
        break;
    }

    m_application->outputWindow()->displayOutput(OutputSource::Application, QString("Running '%1' tool...").arg(tool_to_run->name()));
    tool_to_run->runTool(ptr_editor, data);
  }
}

void ExternalTools::onToolPartialOutputObtained(const QString& output) {
  m_application->outputWindow()->displayOutput(OutputSource::ExternalTool, output, QMessageBox::Icon::Information);
}

void ExternalTools::onToolFinished(const QPointer<TextEditor>& editor, const QString& output_text,
                                   const QString& error_text, bool success) {
  Q_UNUSED(success)

  if (editor.isNull()) {
    qCritical("Cannot work properly with tool output, assigned text editor was already destroyed, dumping text to output toolbox.");
    m_application->outputWindow()->displayOutput(OutputSource::ExternalTool,
                                                 tr("Cannot deliver output of external tool, assigned text editor no longer exists."),
                                                 QMessageBox::Icon::Critical);
    return;
  }

  ExternalTool* tool = qobject_cast<ExternalTool*>(sender());

  if (!error_text.isEmpty()) {
    m_application->outputWindow()->displayOutput(OutputSource::ExternalTool, error_text, QMessageBox::Icon::Critical);
  }

  switch (tool->output()) {
    case ToolOutput::InsertAtCursorPosition: {
      if (!output_text.isEmpty()) {
        QByteArray output_utf = output_text.toUtf8();

        editor->insertText(editor->currentPos(), output_utf.constData());
        editor->gotoPos(editor->currentPos() + output_utf.size());
      }

      break;
    }

    case ToolOutput::ReplaceCurrentLine: {
      if (!output_text.isEmpty()) {
        QByteArray output_utf = output_text.toUtf8();
        auto line = editor->lineFromPosition(editor->currentPos());
        auto start_line = editor->positionFromLine(line);
        auto end_line = editor->lineEndPosition(line);

        editor->setSel(start_line, end_line);
        editor->replaceSel(output_utf);
      }

      break;
    }

    case ToolOutput::CopyToClipboard:
      if (!output_text.isEmpty()) {
        qApp->clipboard()->setText(output_text, QClipboard::Mode::Clipboard);
        m_application->outputWindow()->displayOutput(OutputSource::ExternalTool,
                                                     tr("Tool '%1' finished, output copied to clipboard.").arg(tool->name()),
                                                     QMessageBox::Icon::Information);
      }

      break;

    case ToolOutput::DumpToOutputWindow:
      if (!output_text.isEmpty()) {
        m_application->outputWindow()->displayOutput(OutputSource::ExternalTool, output_text, QMessageBox::Icon::Information);
      }

      break;

    case ToolOutput::NewSavedFile: {
      if (!output_text.isEmpty()) {
        m_application->outputWindow()->displayOutput(OutputSource::ExternalTool,
                                                     tr("Tool '%1' finished, opening output in new tab.").arg(tool->name()),
                                                     QMessageBox::Icon::Information);

        m_application->loadTextEditorFromFile(IOFactory::writeToTempFile(output_text.toUtf8()), DEFAULT_TEXT_FILE_ENCODING);
      }

      break;
    }

    case ToolOutput::ReloadFile:
      editor->reloadFromDisk();
      break;

    case ToolOutput::ReplaceSelectionDocument:
      if (!output_text.isEmpty()) {
        if (!editor->selectionEmpty()) {
          editor->replaceSel(output_text.toUtf8().constData());
        }
        else {
          editor->setText(output_text.toUtf8().constData());
        }
      }

      break;

    case ToolOutput::NoOutput:
    default:
      break;
  }
}
