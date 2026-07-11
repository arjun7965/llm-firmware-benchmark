export type CacheLoader<K, V> = (key: K) => V | PromiseLike<V>;

export interface SingleflightCacheOptions {
  ttlMs: number;
  now?: () => number;
}

export interface AsyncCache<K, V> {
  get(key: K, loader: CacheLoader<K, V>, signal?: AbortSignal): Promise<V>;
  invalidate(key: K): void;
  clear(): void;
}
