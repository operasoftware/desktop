/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TypingCommand_h
#define TypingCommand_h

#include "core/editing/commands/CompositeEditCommand.h"

namespace blink {

class CORE_EXPORT TypingCommand final : public CompositeEditCommand {
 public:
  enum ETypingCommand {
    kDeleteSelection,
    kDeleteKey,
    kForwardDeleteKey,
    kInsertText,
    kInsertLineBreak,
    kInsertParagraphSeparator,
    kInsertParagraphSeparatorInQuotedContent
  };

  enum TextCompositionType {
    kTextCompositionNone,
    kTextCompositionUpdate,
    kTextCompositionConfirm,
    kTextCompositionCancel
  };

  enum Option {
    kSelectInsertedText = 1 << 0,
    kKillRing = 1 << 1,
    kRetainAutocorrectionIndicator = 1 << 2,
    kPreventSpellChecking = 1 << 3,
    kSmartDelete = 1 << 4
  };
  typedef unsigned Options;

  static void DeleteSelection(Document&, Options = 0);
  static void DeleteKeyPressed(Document&,
                               Options,
                               TextGranularity = kCharacterGranularity);
  static void ForwardDeleteKeyPressed(Document&,
                                      EditingState*,
                                      Options = 0,
                                      TextGranularity = kCharacterGranularity);
  static void InsertText(Document&,
                         const String&,
                         Options,
                         TextCompositionType = kTextCompositionNone,
                         const bool is_incremental_insertion = false);
  static void InsertText(
      Document&,
      const String&,
      const SelectionInDOMTree&,
      Options,
      TextCompositionType = kTextCompositionNone,
      const bool is_incremental_insertion = false,
      InputEvent::InputType = InputEvent::InputType::kInsertText);
  static bool InsertLineBreak(Document&);
  static bool InsertParagraphSeparator(Document&);
  static bool InsertParagraphSeparatorInQuotedContent(Document&);
  static void CloseTyping(LocalFrame*);

  static TypingCommand* LastTypingCommandIfStillOpenForTyping(LocalFrame*);

  void InsertText(const String& text, bool select_inserted_text, EditingState*);
  void InsertTextRunWithoutNewlines(const String& text,
                                    bool select_inserted_text,
                                    EditingState*);
  void InsertLineBreak(EditingState*);
  void InsertParagraphSeparatorInQuotedContent(EditingState*);
  void InsertParagraphSeparator(EditingState*);
  void DeleteKeyPressed(TextGranularity, bool kill_ring, EditingState*);
  void ForwardDeleteKeyPressed(TextGranularity, bool kill_ring, EditingState*);
  void DeleteSelection(bool smart_delete, EditingState*);
  void SetCompositionType(TextCompositionType type) {
    composition_type_ = type;
  }
  void AdjustSelectionAfterIncrementalInsertion(LocalFrame*,
                                                const size_t text_length);

  ETypingCommand CommandTypeOfOpenCommand() const { return command_type_; }
  TextCompositionType CompositionType() const { return composition_type_; }
  // |TypingCommand| may contain multiple |InsertTextCommand|, should return
  // |textDataForInputEvent()| of the last one.
  String TextDataForInputEvent() const final;

 private:
  static TypingCommand* Create(
      Document& document,
      ETypingCommand command,
      const String& text = "",
      Options options = 0,
      TextGranularity granularity = kCharacterGranularity) {
    return new TypingCommand(document, command, text, options, granularity,
                             kTextCompositionNone);
  }

  static TypingCommand* Create(Document& document,
                               ETypingCommand command,
                               const String& text,
                               Options options,
                               TextCompositionType composition_type) {
    return new TypingCommand(document, command, text, options,
                             kCharacterGranularity, composition_type);
  }

  TypingCommand(Document&,
                ETypingCommand,
                const String& text,
                Options,
                TextGranularity,
                TextCompositionType);

  void SetSmartDelete(bool smart_delete) { smart_delete_ = smart_delete; }
  bool IsOpenForMoreTyping() const { return open_for_more_typing_; }
  void CloseTyping() { open_for_more_typing_ = false; }

  void DoApply(EditingState*) override;
  InputEvent::InputType GetInputType() const override;
  bool IsTypingCommand() const override;
  bool PreservesTypingStyle() const override { return preserves_typing_style_; }
  void SetShouldRetainAutocorrectionIndicator(bool retain) override {
    should_retain_autocorrection_indicator_ = retain;
  }
  void SetShouldPreventSpellChecking(bool prevent) {
    should_prevent_spell_checking_ = prevent;
  }

  static void UpdateSelectionIfDifferentFromCurrentSelection(TypingCommand*,
                                                             LocalFrame*);

  void UpdatePreservesTypingStyle(ETypingCommand);
  void TypingAddedToOpenCommand(ETypingCommand);
  bool MakeEditableRootEmpty(EditingState*);

  void UpdateCommandTypeOfOpenCommand(ETypingCommand typing_command) {
    command_type_ = typing_command;
  }

  bool IsIncrementalInsertion() const { return is_incremental_insertion_; }

  void DeleteSelectionIfRange(const VisibleSelection&,
                              EditingState*,
                              bool smart_delete = false,
                              bool merge_blocks_after_delete = true,
                              bool expand_for_special_elements = true,
                              bool sanitize_markup = true);

  ETypingCommand command_type_;
  String text_to_insert_;
  bool open_for_more_typing_;
  bool select_inserted_text_;
  bool smart_delete_;
  TextGranularity granularity_;
  TextCompositionType composition_type_;
  bool kill_ring_;
  bool preserves_typing_style_;

  // Undoing a series of backward deletes will restore a selection around all of
  // the characters that were deleted, but only if the typing command being
  // undone was opened with a backward delete.
  bool opened_by_backward_delete_;

  bool should_retain_autocorrection_indicator_;
  bool should_prevent_spell_checking_;

  bool is_incremental_insertion_;
  size_t selection_start_;
  InputEvent::InputType input_type_;
};

DEFINE_TYPE_CASTS(TypingCommand,
                  CompositeEditCommand,
                  command,
                  command->IsTypingCommand(),
                  command.IsTypingCommand());

}  // namespace blink

#endif  // TypingCommand_h
