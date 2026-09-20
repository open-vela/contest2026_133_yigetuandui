export class EventBus {
  #listeners = new Map();

  on(eventName, handler) {
    const handlers = this.#listeners.get(eventName) || new Set();
    handlers.add(handler);
    this.#listeners.set(eventName, handlers);
    return () => handlers.delete(handler);
  }

  emit(eventName, payload) {
    for (const handler of this.#listeners.get(eventName) || []) handler(payload);
  }

  clear(eventName) {
    if (eventName) this.#listeners.delete(eventName);
    else this.#listeners.clear();
  }
}

export const eventBus = new EventBus();
