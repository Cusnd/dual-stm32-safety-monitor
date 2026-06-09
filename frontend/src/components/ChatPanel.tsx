import { Button, Card, Group, ScrollArea, Text, TextInput, Title } from "@mantine/core";
import { SendHorizonal } from "lucide-react";
import { FormEvent, useEffect, useRef, useState } from "react";
import type { ReactNode } from "react";
import { MarkdownMessage } from "./MarkdownMessage";
import type { DashboardState } from "../hooks/useDashboard";

export function ChatPanel({
  dashboard,
  framed = true,
  draft: controlledDraft,
  onDraftChange,
  scrollHeight = 270,
}: {
  dashboard: DashboardState;
  framed?: boolean;
  draft?: string;
  onDraftChange?: (value: string) => void;
  scrollHeight?: number;
}) {
  const [localDraft, setLocalDraft] = useState("");
  const draft = controlledDraft ?? localDraft;
  const viewportRef = useRef<HTMLDivElement>(null);
  const setDraft = (value: string) => {
    if (onDraftChange) {
      onDraftChange(value);
    } else {
      setLocalDraft(value);
    }
  };

  useEffect(() => {
    const viewport = viewportRef.current;
    if (viewport) {
      viewport.scrollTop = viewport.scrollHeight;
    }
  }, [dashboard.chatMessages.length]);

  const submit = (event: FormEvent) => {
    event.preventDefault();
    const content = draft.trim();
    if (!content) {
      return;
    }
    setDraft("");
    void dashboard.sendChat(content);
  };

  const content: ReactNode = (
    <>
      <Title order={2}>{dashboard.t("aiChat")}</Title>
      <ScrollArea h={scrollHeight} viewportRef={viewportRef} mt="sm" type="hover">
        <ol className="chat-list" aria-live="polite">
          {dashboard.chatMessages.map((message) => (
            <li key={message.id} className={`chat-message role-${message.role}${message.pending ? " pending" : ""}`}>
              <Group justify="space-between" gap="xs" mb={4}>
                <Text size="xs" fw={800}>
                  {message.role === "user" ? dashboard.t("you") : message.provider ?? "AI"}
                </Text>
              </Group>
              {message.role === "assistant" ? (
                <MarkdownMessage content={message.content} />
              ) : (
                <Text className="message-content">{message.content}</Text>
              )}
            </li>
          ))}
        </ol>
      </ScrollArea>
      <form className="chat-form" onSubmit={submit}>
        <TextInput
          value={draft}
          onChange={(event) => setDraft(event.currentTarget.value)}
          placeholder={dashboard.t("chatPlaceholder")}
          disabled={dashboard.chatBusy}
        />
        <Button type="submit" leftSection={<SendHorizonal size={16} />} loading={dashboard.chatBusy}>
          {dashboard.chatBusy ? dashboard.t("sending") : dashboard.t("send")}
        </Button>
      </form>
    </>
  );

  if (!framed) {
    return <section className="assistant-section chat-panel">{content}</section>;
  }

  return (
    <Card className="panel-card chat-panel" withBorder>
      {content}
    </Card>
  );
}
