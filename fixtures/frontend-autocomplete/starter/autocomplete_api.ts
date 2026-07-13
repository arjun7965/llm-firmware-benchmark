import type { ComponentType } from "react";

export interface User {
  id: string;
  name: string;
}

export type SearchUsers = (
  query: string,
  signal: AbortSignal,
) => Promise<User[]>;

export interface AutocompleteProps {
  searchUsers: SearchUsers;
}

export type AutocompleteComponent = ComponentType<AutocompleteProps>;
