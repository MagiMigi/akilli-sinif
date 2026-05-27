import React from 'react';
import { StyleSheet, Text, View } from 'react-native';
import Svg, { Defs, LinearGradient, Path, Stop, Line, Text as SvgText } from 'react-native-svg';
import { colors, radius, spacing, typography } from '@/theme';

interface Props {
  data: number[];
  height?: number;
  color?: string;
  unit?: string;
  title?: string;
}

const PADDING_X = 8;
const PADDING_TOP = 16;
const PADDING_BOTTOM = 8;

export function LineChart({
  data,
  height = 180,
  color = colors.accent,
  unit,
  title,
}: Props) {
  const [width, setWidth] = React.useState(0);

  if (!data || data.length < 2) {
    return (
      <View style={[styles.container, { height }]}>
        {title ? <Text style={styles.title}>{title}</Text> : null}
        <View style={styles.empty}>
          <Text style={styles.emptyText}>Yeterli veri yok</Text>
        </View>
      </View>
    );
  }

  const min = Math.min(...data);
  const max = Math.max(...data);
  const range = max - min || 1;
  const innerH = height - PADDING_TOP - PADDING_BOTTOM;
  const innerW = Math.max(1, width - PADDING_X * 2);
  const stepX = innerW / (data.length - 1);

  const path = data
    .map((v, i) => {
      const x = PADDING_X + i * stepX;
      const y = PADDING_TOP + (1 - (v - min) / range) * innerH;
      return `${i === 0 ? 'M' : 'L'}${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(' ');

  const fill = `${path} L${PADDING_X + (data.length - 1) * stepX},${
    PADDING_TOP + innerH
  } L${PADDING_X},${PADDING_TOP + innerH} Z`;

  const last = data[data.length - 1];
  const formatted = `${last.toFixed(unit === 'C' || unit === '%' ? 1 : 0)}${unit ?? ''}`;

  return (
    <View
      style={styles.container}
      onLayout={(e) => setWidth(e.nativeEvent.layout.width)}
    >
      <View style={styles.header}>
        {title ? <Text style={styles.title}>{title}</Text> : null}
        <Text style={styles.current}>{formatted}</Text>
      </View>
      {width > 0 ? (
        <Svg width={width} height={height}>
          <Defs>
            <LinearGradient id="grad" x1="0" y1="0" x2="0" y2="1">
              <Stop offset="0" stopColor={color} stopOpacity={0.3} />
              <Stop offset="1" stopColor={color} stopOpacity={0} />
            </LinearGradient>
          </Defs>
          <Line
            x1={PADDING_X}
            x2={width - PADDING_X}
            y1={PADDING_TOP + innerH / 2}
            y2={PADDING_TOP + innerH / 2}
            stroke={colors.border}
            strokeDasharray="3 5"
          />
          <Path d={fill} fill="url(#grad)" />
          <Path d={path} stroke={color} strokeWidth={2.5} fill="none" />
          <SvgText
            x={PADDING_X}
            y={PADDING_TOP - 4}
            fill={colors.muted}
            fontSize={10}
          >
            {`${max.toFixed(1)}${unit ?? ''}`}
          </SvgText>
          <SvgText
            x={PADDING_X}
            y={PADDING_TOP + innerH + 8}
            fill={colors.muted}
            fontSize={10}
          >
            {`${min.toFixed(1)}${unit ?? ''}`}
          </SvgText>
        </Svg>
      ) : null}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    backgroundColor: colors.surface,
    borderRadius: radius.lg,
    padding: spacing.md,
    borderWidth: 1,
    borderColor: colors.border,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'baseline',
    marginBottom: spacing.sm,
  },
  title: {
    ...typography.bodyStrong,
  },
  current: {
    ...typography.h3,
    color: colors.accent,
  },
  empty: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
  },
  emptyText: {
    ...typography.caption,
  },
});
