import React, { useRef } from 'react';
import {
  GestureResponderEvent,
  PanResponder,
  StyleSheet,
  Text,
  View,
} from 'react-native';
import { colors, radius, spacing, typography } from '@/theme';

interface Props {
  label: string;
  value: number;
  min?: number;
  max?: number;
  unit?: string;
  accent?: string;
  onChange: (next: number) => void;
  onCommit?: (next: number) => void;
}

export function Slider({
  label,
  value,
  min = 0,
  max = 100,
  unit = '%',
  accent = colors.accent,
  onChange,
  onCommit,
}: Props) {
  const trackWidth = useRef(1);
  const lastValue = useRef(value);

  const update = (locationX: number, commit: boolean) => {
    const w = trackWidth.current;
    const ratio = Math.max(0, Math.min(1, locationX / w));
    const next = Math.round(min + (max - min) * ratio);
    if (next !== lastValue.current) {
      lastValue.current = next;
      onChange(next);
    }
    if (commit) onCommit?.(next);
  };

  const responder = useRef(
    PanResponder.create({
      onStartShouldSetPanResponder: () => true,
      onMoveShouldSetPanResponder: () => true,
      onPanResponderGrant: (e) => update(e.nativeEvent.locationX, false),
      onPanResponderMove: (e) => update(e.nativeEvent.locationX, false),
      onPanResponderRelease: (e) => update(e.nativeEvent.locationX, true),
    })
  ).current;

  const ratio = (value - min) / (max - min || 1);

  return (
    <View style={styles.container}>
      <View style={styles.head}>
        <Text style={styles.label}>{label}</Text>
        <Text style={[styles.value, { color: accent }]}>
          {value}
          {unit}
        </Text>
      </View>
      <View
        style={styles.track}
        onLayout={(e) => (trackWidth.current = e.nativeEvent.layout.width)}
        {...responder.panHandlers}
      >
        <View
          style={[
            styles.fill,
            { width: `${ratio * 100}%`, backgroundColor: accent },
          ]}
        />
        <View
          style={[
            styles.thumb,
            { left: `${ratio * 100}%`, borderColor: accent },
          ]}
        />
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    gap: spacing.sm,
  },
  head: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'baseline',
  },
  label: {
    ...typography.bodyStrong,
  },
  value: {
    ...typography.h2,
  },
  track: {
    height: 12,
    backgroundColor: colors.surfaceAlt,
    borderRadius: radius.pill,
    borderWidth: 1,
    borderColor: colors.border,
    overflow: 'visible',
    justifyContent: 'center',
  },
  fill: {
    position: 'absolute',
    left: 0,
    top: 0,
    bottom: 0,
    borderRadius: radius.pill,
  },
  thumb: {
    position: 'absolute',
    width: 24,
    height: 24,
    borderRadius: 12,
    backgroundColor: colors.text,
    borderWidth: 3,
    marginLeft: -12,
    top: -6,
    elevation: 3,
  },
});
