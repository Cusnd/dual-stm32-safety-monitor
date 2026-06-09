import ReactMarkdown from "react-markdown";
import remarkGfm from "remark-gfm";

function safeHref(href: string | undefined): string {
  const text = String(href ?? "").trim();
  if (!text) {
    return "";
  }

  try {
    const parsed = new URL(text, globalThis.location?.origin ?? "http://localhost");
    if (parsed.protocol === "http:" || parsed.protocol === "https:") {
      return parsed.href;
    }
  } catch {
    return "";
  }
  return "";
}

export function MarkdownMessage({ content }: { content: string }) {
  const safeContent = content.replaceAll("<", "&lt;").replaceAll(">", "&gt;");

  return (
    <div className="markdown-content">
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        urlTransform={(url, key) => key === "href" ? safeHref(url) : ""}
        components={{
          a({ href, children }) {
            const safe = safeHref(href);
            if (!safe) {
              return <span>{children}</span>;
            }
            return (
              <a href={safe} target="_blank" rel="noreferrer">
                {children}
              </a>
            );
          },
          img() {
            return null;
          },
          code({ className, children, ...props }) {
            return (
              <code className={className} {...props}>
                {children}
              </code>
            );
          },
        }}
      >
        {safeContent}
      </ReactMarkdown>
    </div>
  );
}
